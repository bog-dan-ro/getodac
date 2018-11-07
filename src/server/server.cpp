/*
    Copyright (C) 2018, BogDan Vatra <bogdan@kde.org>

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

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>
#include <stdexcept>

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <grp.h>
#include <malloc.h>
#include <pwd.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <openssl/err.h>
#include <sys/epoll.h>
#include <sys/socket.h>


#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/core.hpp>
#include <boost/log/common.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/filter_parser.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/log/utility/setup/from_settings.hpp>
#include <boost/log/utility/setup/settings.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <getodac/exceptions.h>
#include <getodac/logging.h>

#include "secured_server_session.h"
#include "server.h"
#include "server_logger.h"
#include "server_session.h"
#include "server_service_sessions.h"
#include "sessions_event_loop.h"

#include "x86_64-signal.h"

#if OPENSSL_VERSION_NUMBER < 0x1010000fL
# error "Only SSL 1.1+ is supported"
#endif

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

template<typename T>
using deleted_unique_ptr = std::unique_ptr<T,std::function<void(T*)>>;

namespace Getodac {

TaggedLogger<> serverLogger{"Server"};

namespace {
    uint32_t queuedConnections = 20000;
    uint32_t eventLoopsSize = std::max(uint32_t(2), std::thread::hardware_concurrency());

    std::string stackTrace(int discard = 0)
    {
        void *addresses[100];
        auto count = backtrace(addresses, 100);
        char **symbols = backtrace_symbols(addresses, count);
        std::ostringstream stackTtrace;
        for (int i = discard; i < count; ++i) {
            Dl_info dlinfo;
            if(!dladdr(addresses[i], &dlinfo))
                break;
            const char * symbol = dlinfo.dli_sname;
            int status;
            auto demangled = abi::__cxa_demangle(symbol, NULL, 0, &status);
            if (demangled && !status)
                symbol = demangled;
            if (symbol)
                stackTtrace <<  dlinfo.dli_fname << " (" << symbol << ")" << " [" << addresses[i] << "]" << std::endl;
            else
                stackTtrace << symbols[i] << std::endl;

            if (demangled)
                free(demangled);
        }
        free(symbols);
        return stackTtrace.str();
    }

    static void unblockSignal(int signum)
    {
        sigset_t sigs;
        sigemptyset(&sigs);
        sigaddset(&sigs, signum);
        sigprocmask(SIG_UNBLOCK, &sigs, nullptr);
    }

    std::vector<std::pair<std::string, std::string>> mergedProperties(const boost::property_tree::ptree &node, const std::string &prevNodes = {})
    {
        std::vector<std::pair<std::string, std::string>> res;
        if (node.empty()) {
            res.emplace_back(std::pair<std::string, std::string>{prevNodes, node.data()});
            return res;
        }
        for (const auto &n : node) {
            auto pn = prevNodes;
            if (!pn.empty())
                pn += ".";
            pn += n.first;
            auto props = mergedProperties(n.second, pn);
            res.insert(res.end(), props.begin(), props.end());
        }
        return res;
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
    INFO(serverLogger) << "shutting down the server";
    instance()->m_shutdown.store(true);
}

/// Transform segmentation violations signals into exceptions
SIGNAL_HANDLER(catch_segv)
{
    unblockSignal(SIGSEGV);
    throw SegmentationFaultError(stackTrace(3));
}

/// Transform floation-point errors signals into exceptions
SIGNAL_HANDLER(catch_fpe)
{
    unblockSignal(SIGFPE);
    throw FloatingPointError(stackTrace(3));
}

/// Makes socket nonblocking
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
    if ((sock = ::socket(type == IPV4 ? AF_INET : AF_INET6, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0)) < 0)
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

    if (::listen(sock, queuedConnections) == -1)
        throw std::runtime_error{"Can't listen on the socket"};

    struct epoll_event event;
    event.data.ptr = nullptr;
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

    boost::log::add_common_attributes();
    boost::log::register_simple_filter_factory<boost::log::trivial::severity_level, char>("Severity");
    boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");

    // Server start time, will be used by server sessions
    m_startTime = std::chrono::system_clock::now();

    namespace po = boost::program_options;
    namespace fs = boost::filesystem;
    int httpPort = 8080; // Default HTTP port
    int httpsPort = 8443; // Default HTTPS port

    // Default plugins path
    std::string pluginsPath = fs::canonical(fs::path(argv[0])).parent_path().parent_path().append("/lib/getodac/plugins").string();
    std::string confDir = fs::canonical(fs::path(argv[0])).parent_path().parent_path().append("/conf").string();
    std::string dropUser;
    std::string dropGroup;
    bool printPID = false;
    // Server arguments
    po::options_description desc{"GETodac options"};
    desc.add_options()
            ("conf,c", po::value<std::string>(&confDir), "configurations path")
            ("plugins-dir,d", po::value<std::string>(&pluginsPath)->default_value(pluginsPath), "plugins dir")
            ("workers,w", po::value<uint32_t>(&eventLoopsSize), "configuration file")
            ("user,u", po::value<std::string>(&dropUser), "username to drop privileges to")
            ("group,g", po::value<std::string>(&dropGroup), "optional group to drop privileges to, if missing the main user group will be used")
            ("pid", "print GETodac pid")
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
        printPID = vm.count("pid");
    } catch (po::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        std::cerr << desc << std::endl;
        throw;
    }

    if (eventLoopsSize < 1)
        throw std::runtime_error("Invalid workers count");

    bool enableServerStatus = false;
    gid_t gid = gid_t(-1);
    uid_t uid = uid_t(-1);
    if (!confDir.empty()) {
        auto curPath = boost::filesystem::current_path();
        boost::filesystem::current_path(confDir);
        namespace pt = boost::property_tree;
        pt::ptree properties;
        pt::read_info("server.conf", properties);
        auto loggingSettings = mergedProperties(properties.get_child("logging"));
        boost::log::settings setts;
        for (const auto &kv : loggingSettings)
            setts[kv.first] = kv.second;
        boost::log::init_from_settings(setts);
        TRACE(serverLogger) << "Loading logging settings succeeded";

        enableServerStatus = properties.get("server_status", false);
        httpPort = properties.get("http_port", -1);
        queuedConnections = properties.get("queued_connections", queuedConnections);
        TRACE(serverLogger) << "http port:" << httpPort;
        if (properties.find("https") != properties.not_found()) {
            TRACE(serverLogger) << "https section found in config";
            if (properties.get("https.enabled", false)) {
                httpsPort = properties.get("https.port", httpsPort);
                TRACE(serverLogger) << "https enabled in configm port=" << httpsPort;

                // Init SSL Context
                SSL_library_init();
                SSL_load_error_strings();
                OpenSSL_add_all_algorithms();
                std::string ctxMethod = boost::algorithm::to_lower_copy(properties.get<std::string>("https.ssl.ctx_method"));
                DEBUG(serverLogger) << "SSL_CTX_new(" << ctxMethod << ")";
                if (!(m_SSLContext = SSL_CTX_new(ctxMethod == "DTLS" ? DTLS_server_method() : TLS_server_method())))
                    throw std::runtime_error("Can't create SSL Context");

                // load SSL CTX configuration
                auto ctxConf = deleted_unique_ptr<SSL_CONF_CTX>(SSL_CONF_CTX_new(), [](SSL_CONF_CTX *ptr){SSL_CONF_CTX_free(ptr);});
                if (!ctxConf)
                    throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));
                SSL_CONF_CTX_set_ssl_ctx(ctxConf.get(), m_SSLContext);
                SSL_CONF_CTX_set_flags(ctxConf.get(), SSL_CONF_FLAG_FILE | SSL_CONF_FLAG_SERVER | SSL_CONF_FLAG_CERTIFICATE | SSL_CONF_FLAG_REQUIRE_PRIVATE | SSL_CONF_FLAG_SHOW_ERRORS);

                auto cxt_settings = mergedProperties(properties.get_child("https.ssl.cxt_settings"));
                for (const auto &kv : cxt_settings) {
                    DEBUG(serverLogger) << "SSL_CONF_cmd(" << kv.first << ", " << kv.second << ")";
                    if (SSL_CONF_cmd(ctxConf.get(), kv.first.c_str(), kv.second.empty() ? nullptr : kv.second.c_str()) < 1)
                        throw std::runtime_error{ERR_error_string(ERR_get_error(), nullptr)};
                }

                if (SSL_CONF_CTX_finish(ctxConf.get()) != 1 || SSL_CTX_check_private_key(m_SSLContext) != 1)
                    throw std::runtime_error(ERR_error_string(ERR_get_error(), nullptr));

            } else {
                httpsPort = -1;
            }

            if (!getuid() && (!dropUser.empty() ||
                    (properties.find("privileges") != properties.not_found() && properties.get("privileges.drop", false)))) {
                auto usr = dropUser.empty() ? properties.get<std::string>("privileges.user") : dropUser;
                auto user = getpwnam(usr.c_str());
                if (!user)
                    throw std::runtime_error("Can't find user \"" + usr + "\"");
                uid = user->pw_uid;
                gid = user->pw_gid;
                auto grp = dropGroup.empty() ? properties.get<std::string>("privileges.group") : dropGroup;
                if (!grp.empty()) {
                    auto group =getgrnam(grp.c_str());
                    if (!group)
                        throw std::runtime_error("Can't find group \"" + grp + "\"");
                    gid = group->gr_gid;
                }
            }
        }
        boost::filesystem::current_path(curPath);
    }

    if (httpPort < 0 && httpsPort < 0)
        throw std::runtime_error{"No HTTP nor HTTPS ports specified"};

    // load plugins
    if (fs::is_directory(pluginsPath)) {
        fs::directory_iterator end_iter;
        for (fs::directory_iterator dir_itr{pluginsPath}; dir_itr != end_iter; ++dir_itr) {
            try {
                if (fs::is_regular_file(dir_itr->status()))
                    m_plugins.emplace_back(dir_itr->path().string(), confDir);
            } catch (const std::exception &e) {
                ERROR(serverLogger) << e.what();
            }
        }
    }

    // at the end add the server sessions
    if (enableServerStatus)
        m_plugins.emplace_back(&ServerSessions::createSession, UINT32_MAX / 2);
    std::sort(m_plugins.begin(), m_plugins.end(), [](const ServerPlugin &a, const ServerPlugin &b){return a.order() < b.order();});

    // accept thread must have insane priority to be able to accept connections
    // as fast as possible
    sched_param sch;
    sch.sched_priority = sched_get_priority_max(SCHED_RR);
    pthread_setschedparam(pthread_self(), SCHED_RR, &sch);

    // Bind IPv4 & IPv6 http ports
    if (httpPort > 0) {
        bind(IPV4, httpPort);
        bind(IPV6, httpPort);
        INFO(serverLogger) << "listen on :"<< httpPort << " port";
    }

    if (httpsPort > 0) {
        // Bind IPv4 & IPv6 https ports
        https4Sock = bind(IPV4, httpsPort);
        https6Sock = bind(IPV6, httpsPort);
        INFO(serverLogger) << "listen on :"<< httpsPort << " port";
    }

    if (!getuid() && gid != gid_t(-1) && uid != uid_t(-1)) {
        if (setgid(gid) || setuid(uid))
             throw std::runtime_error("Can't drop privileges");
        INFO(serverLogger) << "Droping privileges";
    }

    auto eventLoops = std::make_unique<SessionsEventLoop[]>(eventLoopsSize);

    INFO(serverLogger) << "using " << eventLoopsSize << " worker threads";

    INFO(serverLogger) << "using " << queuedConnections << " queued connections";

    // allocate epoll list
    const auto epollList = std::make_unique<epoll_event[]>(m_eventsSize);

    if (printPID)
        std::cout << "pid:" << getpid() << std::endl << std::flush;

    // Wait for incoming connections
    while (!m_shutdown) {
        int triggeredEvents = epoll_wait(m_epollHandler, epollList.get(), m_eventsSize, 1000);
        {
            std::unique_lock<SpinLock> lock{m_activeSessionsMutex};
            auto sessions = m_activeSessions.size();
            if (sessions > m_peakSessions)
                m_peakSessions = sessions;

            if (sessions <= 1) // No more pending sessions?
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
                    int sock = ::accept4(fd, (struct sockaddr *)&in_addr, &in_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
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
                            m_activeSessions.insert((new SecuredServerSession(bestLoop, sock, in_addr))->sessionReady());
                        else
                            m_activeSessions.insert((new ServerSession(bestLoop, sock, in_addr))->sessionReady());
                    } catch (const std::exception &e) {
                        WARNING(serverLogger) << " Can't create session, reason: " << e.what();
                        ::close(sock);
                    } catch (...) {
                        // if we can't create a new session
                        // then just close the socket
                        WARNING(serverLogger) << " Can't create session, for unknown reason";
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
    m_epollHandler = epoll_create1(EPOLL_CLOEXEC);

    // register signal handlers
    if (signal(SIGINT, Server::exitSignalHandler) == SIG_ERR)
        throw std::runtime_error{"Can't register SIGINT signal callback"};

    if (signal(SIGTERM, Server::exitSignalHandler) == SIG_ERR)
        throw std::runtime_error{"Can't register SIGTERM signal callback"};

    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        throw std::runtime_error{"Can't set SIGPIPE SIG_IGN"};

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
}

} // namespace Getodac
