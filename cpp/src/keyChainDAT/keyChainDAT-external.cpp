//
//  keyChainDAT-external.cpp
//  keyChainDAT
//
//  Created by Peter Gusev on 7/22/19.
//  Copyright © 2019 Derivative. All rights reserved.
//

#include "keyChainDAT-external.hpp"
#include "keyChainDAT.h"

using namespace touch_ndn;

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{
    
    DLLEXPORT
    void
    FillDATPluginInfo(DAT_PluginInfo *info)
    {
        info->apiVersion = DATCPlusPlusAPIVersion;
        info->customOPInfo.opType->setString("Touchndnkeychain");
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
