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

#include <exception>
#include <iostream>

#include <dracon/logging.h>

#include "server.h"
#include "server_logger.h"

int main(int argc, char*argv[])
{
    try {
        return Getodac::server::instance()->exec(argc, argv);
    } catch (const std::exception &e) {
        FATAL(Getodac::server_logger) << e.what();
        return -1;
    } catch (...) {
        FATAL(Getodac::server_logger) << "Unknown exception";
        return -1;
    }
}
