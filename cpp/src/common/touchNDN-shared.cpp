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

#include "touchNDN-shared.hpp"
#include "baseDAT.hpp"

using namespace std;
using namespace touch_ndn;

map<string, void*> TouchNdnOps;

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
    
}
