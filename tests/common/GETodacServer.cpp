#include "GETodacServer.h"

#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>

namespace Getodac {
namespace Test {

static FILE * s_getodacHandle = nullptr;
static pid_t s_getodacPid = -1;

static pid_t pidof(const char *name)
{
    std::string cmd{"pidof "};
    cmd += name;
    FILE *fp = popen(cmd.c_str(), "r");
    char buff[200];
    memset(buff, 0, 200);
    fread(buff, 1, 200, fp);
    fclose(fp);
    return atoi(buff);
}

void startServer(const std::string &path)
{
    if (pidof("GETodac"))
        return;

    s_getodacHandle = popen(path.c_str(), "r");
    std::string output;
    char buf[8];
    memset(buf, 0, sizeof(buf));
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

void terminateServer()
{
    if (s_getodacPid != -1) {
        kill(s_getodacPid, SIGTERM);
        pclose(s_getodacHandle);
    }
}

} // namespace Test
} // namespace Getodac
