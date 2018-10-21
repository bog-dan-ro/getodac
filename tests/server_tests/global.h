#pragma once

#include <string>
#include <unordered_map>
#include <curl/curl.h>

void startGetodacServer(const std::string &path);
void terminateGetodacServer();

class EasyCurl
{
public:
    using Headers = std::unordered_map<std::string, std::string>;
    struct Response
    {
        std::string status;
        Headers headers;
        std::string body;
    };
public:
    EasyCurl();
    ~EasyCurl();
    EasyCurl &setUrl(const std::string &url);
    EasyCurl &setHeaders(const Headers &headers);
    Response request(const std::string &method, std::string upload = {}) const;
    inline Response get() const { return request("GET"); }
    inline Response del() const { return request("DELETE"); }
    inline Response opt() const { return request("OPTIONS"); }
    inline Response post(const std::string &upload) const { return request("POST", upload); }
    inline Response put(const std::string &upload) const { return request("PUT", upload); }

private:
    static size_t read_callback(char *buffer, size_t size, size_t nitems, std::string *upload);
    static size_t write_callback(char *ptr, size_t size, size_t nmemb, Response *self);
    static size_t header_callback(char *buffer, size_t size, size_t nitems, Response *response);
    template<typename ...Args>
    void setOpt(CURLoption option, Args ...args) const {
        if (curl_easy_setopt(m_curl, option, args...) != CURLE_OK)
            throw std::runtime_error{"Can't curl_easy_setopt"};
    }
private:
    CURL *m_curl;
};

