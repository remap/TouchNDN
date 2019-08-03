/**
 * Copyright (C) 2019 Regents of the University of California.
 * @author: Peter Gusev <peter@remap.ucla.edu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the additional exemption that
 * compiling, linking, and/or using OpenSSL is allowed.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * A copy of the GNU Lesser General Public License is in the file COPYING.
 */

#ifndef touchNDN_shared_hpp
#define touchNDN_shared_hpp

#include <stdio.h>
#include <map>
#include <string>
#include <vector>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include <spdlog/spdlog.h>

#define TLOG_TRACE SPDLOG_TRACE
#define TLOG_DEBUG SPDLOG_DEBUG
#define TLOG_INFO SPDLOG_INFO
#define TLOG_WARN SPDLOG_WARNING
#define TLOG_ERROR SPDLOG_ERROR

#define TLOG_LOGGER_TRACE SPDLOG_LOGGER_TRACE
#define TLOG_LOGGER_DEBUG SPDLOG_LOGGER_DEBUG
#define TLOG_LOGGER_INFO SPDLOG_LOGGER_INFO
#define TLOG_LOGGER_WARN SPDLOG_LOGGER_WARN
#define TLOG_LOGGER_ERROR SPDLOG_LOGGER_ERROR
#define TLOG_LOGGER_CRITICAL SPDLOG_LOGGER_CRITICAL

#define TLOG_TRACE_TAG(tag, ...) (TLOG_TRACE(tag##__VA_ARGS__))

#include "config.hpp"

namespace touch_ndn {
    class BaseDAT;
    namespace helpers {
        typedef spdlog::logger logger;
        typedef spdlog::level::level_enum log_level;
    }

    bool saveOp(std::string path, void* op);
    void* retrieveOp(std::string path);
    bool updateOp(std::string path, void* caller, std::string newPath);
    bool eraseOp(std::string path, void* caller);
    std::vector<std::string> getOpList();

    void newLogger(std::string loggerName);
    std::shared_ptr<helpers::logger> getLogger(std::string loggerName);
    void flushLogger(std::string loggerName);
}

#endif /* touchNDN_shared_hpp */
