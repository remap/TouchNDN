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

#include "keyChainDAT-external.hpp"
#include "keyChainDAT.h"

#define MODULE_LOGGER "keyChainDAT"

using namespace std;
using namespace touch_ndn;

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{
    __attribute__((constructor)) void lib_ctor() {
        newLogger(MODULE_LOGGER);
    }
    
    __attribute__((destructor)) void lib_dtor() {
        flushLogger(MODULE_LOGGER);
    }
    
    DLLEXPORT
    void
    FillDATPluginInfo(DAT_PluginInfo *info)
    {
        info->apiVersion = DATCPlusPlusAPIVersion;
        info->customOPInfo.opType->setString("Ndnkeychain");
        info->customOPInfo.opLabel->setString("KeyChain DAT");
        info->customOPInfo.opIcon->setString("KDT");
        info->customOPInfo.authorName->setString("Peter Gusev");
        info->customOPInfo.authorEmail->setString("peter@remap.ucla.edu");
        info->customOPInfo.minInputs = 0;
        info->customOPInfo.maxInputs = 1;
    }
    
    DLLEXPORT
    DAT_CPlusPlusBase*
    CreateDATInstance(const OP_NodeInfo* info)
    {
        return new KeyChainDAT(info);
    }
    
    DLLEXPORT
    void
    DestroyDATInstance(DAT_CPlusPlusBase* instance)
    {
        // Delete the instance here, this will be called when
        // Touch is shutting down, when the DAT using that instance is deleted, or
        // if the DAT loads a different DLL
        delete (KeyChainDAT*)instance;
    }
    
};


namespace touch_ndn {
    shared_ptr<helpers::logger> getModuleLogger()
    {
        return getLogger(MODULE_LOGGER);
    }
}
