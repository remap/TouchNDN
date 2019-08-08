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

#include "helper.hpp"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

using namespace std;
using namespace touch_ndn;

#define DEFAULT_FORMAT "%E.%f [%12n] [%^%-8l%$] [thread %t] %!() : %v"

// logging level
// could be either:
// - "trace"
// - "debug"
// - "info"
// - "warn"
// - "err"
// - "critical"
#define TLOG_LEVEL_ENV "TOUCHNDN_LOG_LEVEL"
#define TLOG_FORMAT_ENV "TOUCHNDN_LOG_FMT"
#define TLOG_FILE_ENV "TOUCHNDN_LOG_FILE"

namespace touch_ndn {
    void initLibrary();
    void initLogger(shared_ptr<spdlog::logger>);
}

map<string, void*> TouchNdnOps;
shared_ptr<spdlog::logger> mainLogger;
string logFile = "";
string logLevel = "";
once_flag onceFlag;

struct _LibInitializer {
    _LibInitializer() {
        call_once(onceFlag, bind(initLibrary));
    }
} libInitializer = {};

__attribute__((constructor)) void helper_ctor(){
}

__attribute__((destructor)) void helper_dtor(){
}

namespace touch_ndn
{

bool saveOp(string path, void* op)
{
    if (TouchNdnOps.find(path) != TouchNdnOps.end())
        return false;
    TouchNdnOps[path] = op;
    return true;
}

void* retrieveOp(string path)
{
    if (TouchNdnOps.find(path) != TouchNdnOps.end())
        return TouchNdnOps[path];
    return nullptr;
}

bool updateOp(string path, void* caller, string newPath)
{
    void *op = retrieveOp(path);
    if (op == caller && op)
    {
        TouchNdnOps.erase(path);
        TouchNdnOps[newPath] = op;
        return true;
    }

    return false;
}

bool eraseOp(string path, void* caller)
{
    void *op = retrieveOp(path);
    if (op && op == caller)
    {
        TouchNdnOps.erase(path);
        return true;
    }

    return false;
}

vector<string> getOpList()
{
    vector<string> opList;
    for (auto it:TouchNdnOps)
        opList.push_back(it.first);
    return opList;
}

void initLibrary()
{
    logLevel = getenv(TLOG_LEVEL_ENV) ? string(getenv(TLOG_LEVEL_ENV)) : "";
    logFile = getenv(TLOG_FILE_ENV) ? string(getenv(TLOG_FILE_ENV)) : "";

    if (logFile != "")
        mainLogger = spdlog::basic_logger_mt<spdlog::async_factory>("main", logFile);
    else
        mainLogger = spdlog::stdout_color_mt("main");

    spdlog::set_pattern(getenv(TLOG_FORMAT_ENV) ? getenv(TLOG_FORMAT_ENV) : DEFAULT_FORMAT);
    initLogger(mainLogger);

    spdlog::flush_every(std::chrono::seconds(3));
    spdlog::set_default_logger(mainLogger);

    TLOG_INFO("Initialized TouchNDN logging: level {0} file {1}", logLevel, logFile);
    spdlog::default_logger()->flush();
}

void initLogger(shared_ptr<spdlog::logger> logger)
{
    logger->flush_on(spdlog::level::err);
    if (logLevel == "")
        logLevel = "info";

    logger->set_level(spdlog::level::from_str(logLevel));
    logger->info("Initialized logger {}: level {} file {}",  logger->name(),
        spdlog::level::to_short_c_str(logger->level()), logFile);
    logger->flush();
}

void newLogger(std::string loggerName)
{
    shared_ptr<spdlog::logger> logger;

    if (logFile != "")
        logger = spdlog::basic_logger_mt<spdlog::async_factory>(loggerName, logFile);
    else
        logger = spdlog::stdout_color_mt(loggerName);

    initLogger(logger);
}

shared_ptr<spdlog::logger> getLogger(string loggerName)
{
    auto logger = spdlog::get(loggerName);
    return logger;
}

void flushLogger(std::string loggerName)
{
    spdlog::get(loggerName)->flush();
}

}
