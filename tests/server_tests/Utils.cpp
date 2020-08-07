#include "Utils.h"

std::string url(const std::string &type, const std::string &path)
{
    if (type == "http")
        return "http://localhost:8080" + path;
    return "https://localhost:8443" + path;
}

long duration(const std::string &type, long time)
{
    if (type == "https")
        return time + 100;
    return time;
}
