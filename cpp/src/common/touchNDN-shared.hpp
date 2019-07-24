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

namespace touch_ndn {
    class BaseDAT;
    
//    static std::map<std::string, void*> TouchNdnOps;
    
    bool saveOp(std::string path, void* op);
    void* retrieveOp(std::string path);
    bool updateOp(std::string path, void* caller, std::string newPath);
    bool eraseOp(std::string path, void* caller);
}

#endif /* touchNDN_shared_hpp */
