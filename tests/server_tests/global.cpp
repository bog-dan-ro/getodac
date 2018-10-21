#include "global.h"

#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <chrono>

static FILE * s_getodacHandle = nullptr;
static pid_t s_getodacPid = -1;

void startGetodacServer(const std::string &path)
{
    s_getodacHandle = popen(path.c_str(), "r");
    std::string output;
    char buf[8];
    size_t rd;
    using clock = std::chrono::system_clock;
    auto start = clock::now();
    // wait until getodac starts
    while ((rd = fread(buf, 1, sizeof(buf), s_getodacHandle)) > 0) {
        output.append(buf, rd);
        if (s_getodacPid == -1) {
            // extract PID
            auto pos = output.find('\n');
            if (pos != std::string::npos) {
                s_getodacPid = std::strtoll(output.substr(4, pos - 4).c_str(), nullptr, 10);
            }
        }
        if (s_getodacPid != -1 && output.find("Using:") != std::string::npos)
            break;

        using namespace std::chrono_literals;
        if (clock::now() - start > 10s) {
            std::cerr << "Can't start GETodac\n";
            exit(1);
        }
    }
}

void terminateGetodacServer()
{
    kill(s_getodacPid, SIGTERM);
    pclose(s_getodacHandle);
}

EasyCurl::EasyCurl()
{
    m_curl = curl_easy_init();
    if (!m_curl)
        throw std::runtime_error{"Can't init CUrl"};
    setOpt(CURLOPT_WRITEFUNCTION, &write_callback);
    setOpt(CURLOPT_HEADERFUNCTION, &header_callback);
}

EasyCurl::~EasyCurl()
{
    curl_easy_cleanup(m_curl);
}

EasyCurl &EasyCurl::setUrl(const std::string &url)
{
    setOpt(CURLOPT_URL, url.c_str());
    return *this;
}

EasyCurl &EasyCurl::setHeaders(const EasyCurl::Headers &headers)
{
    struct curl_slist *list = nullptr;
    for (const auto &kv: headers)
        list = curl_slist_append(list, (kv.first + ": " + kv.second).c_str());

    try {
        setOpt(CURLOPT_HTTPHEADER, list);
    } catch(...) {
        curl_slist_free_all(list);
        throw;
    }
    curl_slist_free_all(list);
    return *this;
}

EasyCurl::Response EasyCurl::request(const std::string &method, std::string upload) const
{
    Response res;
    setOpt(CURLOPT_WRITEDATA, &res);
    setOpt(CURLOPT_HEADERDATA, &res);
    setOpt(CURLOPT_CUSTOMREQUEST, method.c_str());

    if (upload.size()) {
        setOpt(CURLOPT_READDATA, &upload);
        setOpt(CURLOPT_READFUNCTION, &read_callback);
        setOpt(CURLOPT_UPLOAD, (void*)1);
        setOpt(CURLOPT_INFILESIZE_LARGE, curl_off_t(upload.size()));
    }

    auto err = curl_easy_perform(m_curl);
    if (err != CURLE_OK) {
        std::cout << curl_easy_strerror(err) << std::endl;
        throw std::runtime_error{curl_easy_strerror(err)};
    }
    return res;
}

size_t EasyCurl::read_callback(char *buffer, size_t size, size_t nitems, std::string *upload)
{
    size_t sz = std::min(size * nitems, upload->size());
    mempcpy(buffer, upload->c_str(), sz);
    return sz;
}

size_t EasyCurl::write_callback(char *ptr, size_t size, size_t nmemb, EasyCurl::Response *response)
{
    response->body.append(ptr, size * nmemb);
    return size * nmemb;
}

size_t EasyCurl::header_callback(char *buffer, size_t size, size_t nitems, EasyCurl::Response *response)
{
    if (size * nitems <= 2)
        return size * nitems;

    std::string header{buffer, size * nitems - 2};
    if (response->status.empty()) {
        std::vector<std::string> status;
        boost::split(status, header, boost::is_any_of(" "));
        response->status = status.size() > 1 ? status[1] : "unknown";
    } else {
        std::vector<std::string> kv;
        boost::split(kv, header, boost::is_any_of(":"));
        std::string key = kv.empty() ? header : kv[0];
        std::string value = kv.size() == 2 ? kv[1].substr(1) : std::string{};
        response->headers.emplace(key, value);
    }
    return size * nitems;
}
