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

#include "baseOP.hpp"

using namespace std;

namespace touch_ndn
{
    extern shared_ptr<spdlog::logger> getModuleLogger();
    
    namespace helpers
    {
        
        vector<string> split(const char *str, string delim)
        {
            vector<string> tokens;
            string s = string(str);
            size_t pos = 0;
            string token;
            
            while ((pos = s.find(delim)) != string::npos)
            {
                token = s.substr(0, pos);
                if (token.size()) tokens.push_back(token);
                s.erase(0, pos+delim.length());
            }
            tokens.push_back(s);
            return tokens;
        }
        
        // collapse all ".." in the path
        string canonical(string path)
        {
            auto comps = split(path.c_str(), "/");
            vector<string> canonized;
            
            for (auto c:comps)
                if (c == "..")
                    canonized.pop_back();
                else
                    canonized.push_back(c);
            
            string canonicalPath;
            for (auto c:canonized)
                canonicalPath += "/" + c;
            return canonicalPath;
        }
        
    }
    
    //******************************************************************************
    OP_Common::OP_Common()
    : errorString_("")
    , warningString_("")
    , infoString_("")
    , isReady_(false)
    , logger_(getModuleLogger())
    {
    }
    
    void
    OP_Common::subscribe(OP_Common *op)
    {
        if (find(listeners_.begin(), listeners_.end(), op) == listeners_.end())
            listeners_.push_back(op);
    }
    
    void
    OP_Common::unsubscribe(OP_Common *op)
    {
        vector<OP_Common*>::iterator it = find(listeners_.begin(), listeners_.end(), op);
        if (it != listeners_.end())
            listeners_.erase(it);
    }
    
    void
    OP_Common::notifyListeners(const std::string &event)
    {
        for (auto o:listeners_)
            o->onOpUpdate(this, event);
    }
    
    string
    OP_Common::getCanonical(const std::string &path) const
    {
        string canonicalPath;
        
        if (path[0] == '/') // if first symbol is "/" -- assume absolute path
            canonicalPath = helpers::canonical(path);
        else if (path.size() > 0) // otherwise -- relative to current OP's path
            canonicalPath = helpers::canonical(opPath_ + path);
        
        return canonicalPath;
    }
    
    void
    OP_Common::extractOpName(std::string opFullPath, std::string &opPath, std::string &opName)
    {
        size_t last = 0, prev = 0, next = 0;
        
        while ((next = opFullPath.find("/", last)) != std::string::npos)
        {
            prev = last;
            last = next + 1;
        }
        
        opName = opFullPath.substr(last);
        opPath = opFullPath.substr(0, last);
    }
    
    bool
    OP_Common::opPathChanged(std::string path, std::string& oldFullPath)
    {
        if ((opPath_ + opName_) != std::string(path))
        {
            oldFullPath = opPath_ + opName_;
            std::string oldName = opName_, oldPath = opPath_;
            extractOpName(path, opPath_, opName_);
            
            opPathUpdated(oldFullPath, oldPath, oldName);
            
            return true;
        }
        
        return false;
    }
    
    void
    OP_Common::clearError(){ setError(""); }
    void
    OP_Common::setError(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        // TODO: due to the bug https://www.derivative.ca/Forum/viewtopic.php?f=12&t=19740#p60455
        // use setString(warningString_, format, args); instead
        // change it when the bug is fixed
        // setString(errorString_, format, args);
        setString(warningString_, format, args);
        va_end(args);
        
        isReady_ = false;
    }
    
    void
    OP_Common::clearWarning() { setWarning(""); }
    void
    OP_Common::setWarning(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        setString(warningString_, format, args);
        va_end(args);
    }
    
    void
    OP_Common::setInfo(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        setString(infoString_, format, args);
        va_end(args);
    }

    void
    OP_Common::setString(std::string &string, const char *format, va_list args)
    {
        static char s[4096];
        memset(s, (char)0, 4096);
        vsprintf(&(s[0]), format, args);
        string = std::string(s);
    }
    
}

