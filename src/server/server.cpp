/*
    Copyright (C) 2016, BogDan Vatra <bogdan@kde.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <malloc.h>

#include <arpa/inet.h>
#include <openssl/err.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <getodac/exceptions.h>

#include "server.h"
#include "server_session.h"
#include "sessions_event_loop.h"
#include "server_service_sessions.h"
#include "secured_server_session.h"

#include "x86_64-signal.h"


void * operator new(std::size_t n)
{
    void *ptr = malloc(n);
    if (!ptr)
        throw std::bad_alloc();
    return ptr;
}

void operator delete(void *p) noexcept
{
    if (p)
        free(p);
}

void operator delete(void *p, std::size_t n) noexcept
{
    (void)n;
    if (p)
        free(p);
}

namespace Getodac {
int Server::SSLDataIndex = -1;

namespace {
    const uint32_t QUEUED_CONNECTIONS = 10000;
    uint32_t eventLoopsSize = std::max(uint32_t(2), std::thread::hardware_concurrency());

    std::string stackTrace()
    {
        void *array[100];
        size_t count = backtrace(array, 100);
        char **symbols = backtrace_symbols(array, count);
        std::ostringstream stackTtrace;
        for (size_t i = 0; i < count; ++i)
            stackTtrace << symbols[i] << std::endl;
        free(symbols);
        return std::move(stackTtrace.str());
    }

    static void unblockSignal(int signum)
    {
        sigset_t sigs;
        sigemptyset(&sigs);
        sigaddset(&sigs, signum);
        sigprocmask(SIG_UNBLOCK, &sigs, NULL);
    }

    class PthreadRW {
    public:
        PthreadRW()
        {
            if (pthread_rwlock_init(&m_lock, nullptr))
                throw std::runtime_error("rwlock_init failed");
        }
        ~PthreadRW()
        {
            pthread_rwlock_destroy(&m_lock);
        }
        inline void lock_read(){ pthread_rwlock_rdlock(&m_lock); }
        inline void lock_write() { pthread_rwlock_wrlock(&m_lock); }
        inline void unlock() { pthread_rwlock_unlock(&m_lock); }
    private:
        pthread_rwlock_t m_lock;
    };

    // SSL Crypto thread stuff
    std::unique_ptr<PthreadRW[]> s_cryptoMutexes;
    void sslThreadLock(int mode, int type, const char */*file*/, int /*line*/)
    {
        if (mode & CRYPTO_UNLOCK) {
            s_cryptoMutexes[type].unlock();
        } else {
            if (mode & CRYPTO_READ)
                s_cryptoMutexes[type].lock_read();
            else
                s_cryptoMutexes[type].lock_write();
        }
    }
    unsigned long sslGetThreadId(void)
    {
        return ::pthread_self();
    }
}

/*!
 * \brief Server::exitSignalHandler
 *
 * Exit the server
 */
void Server::exitSignalHandler(int)
{
    // Quit server loop
    instance()->m_shutdown.store(true);
    std::cout << "Please wait, shutting down the server " << std::flush;
}

/// Transform broken pipe signals into exceptions
SIGNAL_HANDLER(catch_pipe)
{
    unblockSignal(SIGPIPE);
    throw BrokenPipeError(stackTrace());
}

/// Transform segmentation violations signals into exceptions
SIGNAL_HANDLER(catch_segv)
{
    unblockSignal(SIGSEGV);
    throw SegmentationFaultError(stackTrace());
}

/// Transform floation-point errors signals into exceptions
SIGNAL_HANDLER(catch_fpe)
{
    unblockSignal(SIGFPE);
    throw FloatingPointError(stackTrace());
}

/// Makes socket nonblocking
static int nonBlocking(int sock)
{
    int flags = ::fcntl(sock, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error{"F_GETFL error"};

    if (flags & O_NONBLOCK)
        return sock;

    flags |= O_NONBLOCK;
    if (::fcntl(sock, F_SETFL, flags) == -1)
        throw std::runtime_error{"F_SETFL error"};

    return sock;
}

/*!
 * \brief Server::bind
 *
 * \param type the socket type
 * \param port the port that on which will be bound
 *
 * \return the bound socket
 */
int Server::bind(SocketType type, int port)
{
    int sock = -1;
    if ((sock = ::socket(type == IPV4 ? AF_INET : AF_INET6, SOCK_STREAM, 0)) < 0)
        throw std::runtime_error{"Can't create the socket"};

    int opt = 1;
    if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error{"Can't set the socket SO_REUSEADDR option"};

    if (type == IPV4) {
        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family      = AF_INET;
        saddr.sin_addr.s_addr = htonl(INADDR_ANY);
        saddr.sin_port        = htons(port);
        if (::bind(sock,(struct sockaddr *) &saddr, sizeof(saddr)) < 0)
            throw std::runtime_error{"Can't bind the socket"};
    } else {
        opt = 1;
        if (::setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0)
            throw std::runtime_error{"Can't set the socket IPV6_V6ONLY option"};

        struct sockaddr_in6 saddr6;
        memset(&saddr6, 0, sizeof(saddr6));
        saddr6.sin6_addr = in6addr_any;
        saddr6.sin6_port = htons(port);
        saddr6.sin6_family = AF_INET6;
        if (::bind(sock,(struct sockaddr *) &saddr6, sizeof(saddr6)) < 0)
            throw std::runtime_error{"Can't bind the socket"};
    }

    if (::listen(nonBlocking(sock), QUEUED_CONNECTIONS) == -1)
        throw std::runtime_error{"Can't listen on the socket"};

    struct epoll_event event;
    event.data.ptr = 0;
    event.data.fd = sock;
    event.events = EPOLLIN | EPOLLPRI | EPOLLRDHUP | EPOLLET;

    if (epoll_ctl(m_epollHandler, EPOLL_CTL_ADD, sock, &event))
        throw std::runtime_error{"Can't  epoll_ctl"};

    ++m_eventsSize;
    return sock;
}

/*!
 * \brief Server::instance
 *
 * \return the server instance
 */
Server *Server::instance()
{
    static Server server;
    return &server;
}

/*!
 * \brief Server::exec
 *
 * Executes the server loop.
 * The loop is also used to accept incoming connections
 *
 * \param argc main function argc
 * \param argv main function argc
 *
 * \return the status
 */
int Server::exec(int argc, char *argv[])
{
    static bool running = false;
    if (running)
        throw std::runtime_error{"Already running"};

    // Server start time, will be used by server sessions
    m_startTime = std::chrono::system_clock::now();

    namespace po = boost::program_options;
    namespace fs = boost::filesystem;
    int httpPort = 8080; // Default HTTP port
    int httpsPort = 8443; // Default HTTPS port

    // Default plugins path
    std::string pluginsPath = fs::path(argv[0]).parent_path().parent_path().append("/libs/getodac/plugins").string();
    std::string confDir;

    // Server arguments
    po::options_description desc{"GETodac options"};
    desc.add_options()
            ("conf,c", po::value<std::string>(&confDir), "configurations path")
            ("plugins-dir,d", po::value<std::string>(&pluginsPath)->default_value(pluginsPath), "plugins dir")
            ("workers,w", po::value<uint32_t>(&eventLoopsSize), "configuration file")
            ("help,h", "print this help")
            ;

    try {
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
        if (vm.count("help")) {
            std::cout << desc << std::endl;
            exitSignalHandler(0);
            return 0;
        }
    } catch (po::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << desc << std::endl;
        throw;
    }

    if (!eventLoopsSize)
        throw std::runtime_error("Invalid workers count");

    auto eventLoops = std::make_unique<SessionsEventLoop[]>(eventLoopsSize);

    if (!confDir.empty()) {
        namespace pt = boost::property_tree;
        pt::ptree properties;
        pt::read_info(boost::filesystem::path(confDir).append("/server.conf").string(), properties);
        httpPort = properties.get("http_port", httpPort);
        if (properties.find("https") != properties.not_found()) {
            if (properties.get("https.enabled", false)) {
                httpsPort = properties.get("https.port", httpsPort);

                // Init SSL Context
                SSL_library_init();
                SSL_load_error_strings();
                OpenSSL_add_all_algorithms();
                if (!(m_SSLContext = SSL_CTX_new(SSLv23_server_method())))
                        throw std::runtime_error("Can't create SSL Context");

                std::string path = properties.get<std::string>("https.certificate");
                if (SSL_CTX_use_certificate_file(m_SSLContext, path.c_str(), SSL_FILETYPE_PEM) <= 0)
                    throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

                path = properties.get<std::string>("https.privateKey");
                if (SSL_CTX_use_PrivateKey_file(m_SSLContext, path.c_str(), SSL_FILETYPE_PEM) <= 0)
                 throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

                if (SSL_CTX_check_private_key(m_SSLContext) != 1)
                    throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

                std::string ciphers = properties.get("https.ciphers", std::string{});
                if (!ciphers.empty() && ! SSL_CTX_set_cipher_list(m_SSLContext, ciphers.c_str()))
                    throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

                if (properties.get("https.honorCipherOrder", false))
                    SSL_CTX_set_options(m_SSLContext, SSL_OP_CIPHER_SERVER_PREFERENCE);

                if (!properties.get("https.compression", false))
                    SSL_CTX_set_options(m_SSLContext, SSL_OP_NO_COMPRESSION);

                SSLDataIndex = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
            } else {
                httpsPort = 0;
            }
        }
    }

    // load plugins
    if (fs::is_directory(pluginsPath)) {
        fs::directory_iterator end_iter;
        for (fs::directory_iterator dir_itr{pluginsPath}; dir_itr != end_iter; ++dir_itr) {
            try {
                if (fs::is_regular_file(dir_itr->status()))
                    m_plugins.emplace_back(dir_itr->path().string(), confDir);
            } catch (const std::exception &e) {
                std::cerr << e.what() << std::endl;
            }
        }
    }

    // at the end add the server sessions
    m_plugins.emplace_back(&ServerSessions::createSession, UINT32_MAX / 2);
    std::sort(m_plugins.begin(), m_plugins.end(), [](const ServerPlugin &a, const ServerPlugin &b){return a.order() < b.order();});
    // accept thread must have insane priority to be able to accept connections
    // as fast as possible
    sched_param sch;
    sch.sched_priority = sched_get_priority_max(SCHED_RR);
    pthread_setschedparam(pthread_self(), SCHED_RR, &sch);

    // Bind IPv4 & IPv6 http ports
    bind(IPV4, httpPort);
    bind(IPV6, httpPort);
    std::cout << "listen on :"<< httpPort << " port" << std::endl;

    if (httpsPort) {
        // Bind IPv4 & IPv6 https ports
        https4Sock = bind(IPV4, httpsPort);
        https6Sock = bind(IPV6, httpsPort);
        std::cout << "listen on :"<< httpsPort << " port" << std::endl;
    }

    std::cout << "Using:" << eventLoopsSize << " worker threads" << std::endl << std::flush;

    // allocate epoll list
    const auto epollList = std::make_unique<epoll_event[]>(m_eventsSize);

    // Wait for incoming connections
    while (!m_shutdown) {
        int triggeredEvents = epoll_wait(m_epollHandler, epollList.get(), m_eventsSize, 1000);
        {
            std::unique_lock<SpinLock> lock{m_activeSessionsMutex};
            auto sessions = m_activeSessions.size();
            if (sessions > m_peakSessions)
                m_peakSessions = sessions;

            if (!sessions) // No more pending sessions?
                malloc_trim(0); // release the memory to OS
        }

        for (int i = 0; i < triggeredEvents; ++i)
        {
            auto events = epollList[i].events;
            if (events & (EPOLLERR |EPOLLHUP | EPOLLRDHUP))
                throw std::runtime_error{"listen socket error"};

            if (events & (EPOLLIN | EPOLLPRI)) {
                // It's time to accept all connections
                struct sockaddr_storage in_addr;
                socklen_t in_len = sizeof(struct sockaddr_storage);
                while (!m_shutdown) {
                    int fd = epollList[i].data.fd;
                    bool ssl = fd == https4Sock || fd == https6Sock;
                    int sock = ::accept(fd, (struct sockaddr *)&in_addr, &in_len);
                    if (-1 == sock)
                        break;

                    //TODO: here we can check if sock address it's banned
                    //and we can drop the connection

                    // Find the least used session
                    SessionsEventLoop *bestLoop = eventLoops.get();
                    for (uint32_t i = 1; i < eventLoopsSize; ++i) {
                        SessionsEventLoop &loop = eventLoops[i];
                        if (bestLoop->activeSessions() > loop.activeSessions())
                            bestLoop = &loop;
                    }
                    try {
                        // Let's try to create a new session
                        std::unique_lock<SpinLock> lock{m_activeSessionsMutex};
                        if (ssl)
                            m_activeSessions.insert((new SecuredServerSession(bestLoop, nonBlocking(sock), in_addr))->sessionReady());
                        else
                            m_activeSessions.insert((new ServerSession(bestLoop, nonBlocking(sock), in_addr))->sessionReady());
                    } catch (...) {
                        // if we can't create a new session
                        // then just close the socket
                        ::close(sock);
                    }
                }
            }
        }
    }

    // Shutdown event loops
    for (uint32_t i = 0; i < eventLoopsSize; ++i)
        eventLoops[i].shutdown();

    eventLoops.reset();

    // Delete all active sessions
    for (auto &session : m_activeSessions)
        delete session;

    m_plugins.clear();
    return 0;
}

/*!
 * \brief Server::serverSessionDeleted
 *
 * Called by the ServerSession when is deleted
 * \param session that was deleted
 */
void Server::serverSessionDeleted(ServerSession *session)
{
    std::unique_lock<SpinLock> lock{m_activeSessionsMutex};
    m_activeSessions.erase(session);
}

/*!
 * \brief Called by the ServerSession when it has an url and method
 * \return a ServicesSession that can handle the url and the method
 */
std::shared_ptr<AbstractServiceSession> Server::createServiceSession(ServerSession * serverSession, const std::string &url, const std::string &method)
{
    for (const auto &plugin : m_plugins) {
        if (auto service = plugin.createSession(serverSession, url, method))
            return service;
    }
    return std::shared_ptr<AbstractServiceSession>();
}

/*!
 * \brief Server::peakSessions
 * \return the peak of simulatneous connections since the beginning
 */
uint32_t Server::peakSessions()
{
    return m_peakSessions;
}

/*!
 * \brief Server::activeSessions
 * \return the number of active connections
 */
uint32_t Server::activeSessions()
{
    std::unique_lock<SpinLock> lock{m_activeSessionsMutex};
    return m_activeSessions.size();
}

/*!
 * \brief Getodac::Server::uptime
 * \return the number of seconds since the server started
 */
std::chrono::seconds Getodac::Server::uptime() const
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - m_startTime);
}

SSL_CTX *Getodac::Server::sslContext() const
{
    return m_SSLContext;
}

/*!
 * \brief Server::Server
 *
 * Creates the server object & the server events loop
 */
Server::Server()
{
    s_cryptoMutexes = std::make_unique<PthreadRW[]>(CRYPTO_num_locks());
    CRYPTO_set_locking_callback(&sslThreadLock);
    CRYPTO_set_id_callback(&sslGetThreadId);

    m_shutdown.store(false);
    m_epollHandler = epoll_create1(EPOLL_CLOEXEC);

    // register signal handlers
    if (signal(SIGINT, Server::exitSignalHandler) == SIG_ERR)
        throw std::runtime_error{"Can't register SIGINT signal callback"};

    if (signal(SIGTERM, Server::exitSignalHandler) == SIG_ERR)
        throw std::runtime_error{"Can't register SIGTERM signal callback"};

    INIT_PIPE;
    INIT_SEGV;
    INIT_FPE;
}

/*!
 * \brief Server::~Server
 *
 * Cleanup everything
 */
Server::~Server()
{
    if (m_SSLContext)
        SSL_CTX_free(m_SSLContext);
    CRYPTO_set_locking_callback(nullptr);
    CRYPTO_set_id_callback(nullptr);
    std::cout << " done" <<std::endl << std::flush;
}

} // namespace Getodac
