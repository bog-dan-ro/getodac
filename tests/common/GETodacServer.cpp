/*
    Copyright (C) 2020, BogDan Vatra <bogdan@kde.org>

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

    s_getodacHandle = popen((path + " --pid").c_str(), "r");
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    using clock = std::chrono::system_clock;
    auto start = clock::now();
    // wait until getodac starts
    while (fgets(buf, sizeof(buf), s_getodacHandle)) {
        std::string output = buf;
        if (s_getodacPid == -1) {
            // extract PID
            auto pos = output.find('\n');
            if (pos != std::string::npos && output.substr(0, 4) == "pid:") {
                s_getodacPid = std::strtoll(output.substr(4, pos - 4).c_str(), nullptr, 10);
                break;
            }
        }
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
        char buf[1024];
        while (fgets(buf, sizeof(buf), s_getodacHandle));
        pclose(s_getodacHandle);
    }
}

} // namespace Test
} // namespace Getodac
