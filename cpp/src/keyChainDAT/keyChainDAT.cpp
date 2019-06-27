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

#include "keyChainDAT.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <array>
#include <map>

#define PAR_KEYCHAIN_MENU_SIZE 3
#define PAR_KEYCHAIN_MENU "Keychainmenu"
#define PAR_KEYCHAIN_MENU_LABEL "KeyChain Type"
#define PAR_KEYCHAIN_TYPE_SYSTEM "Keychainsystem"
#define PAR_KEYCHAIN_TYPE_FILE "Keychainfile"
#define PAR_KEYCHAIN_TYPE_MEMORY "Keychainmem"
#define PAR_KEYCHAIN_TYPE_SYSTEM_LABEL "System"
#define PAR_KEYCHAIN_TYPE_FILE_LABEL "File"
#define PAR_KEYCHAIN_TYPE_MEMORY_LABEL "Memory"

#define PAR_FACE_DAT "Facedat"
#define PAR_FACE_DAT_LABEL "Face DAT"

using namespace touch_ndn;
using namespace std;
using namespace ndn;

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

//******************************************************************************
const map<string, KeyChainDAT::KeyChainType> KeyChainTypeMap = {
    { PAR_KEYCHAIN_TYPE_SYSTEM, KeyChainDAT::KeyChainType::System },
    { PAR_KEYCHAIN_TYPE_FILE, KeyChainDAT::KeyChainType::File },
    { PAR_KEYCHAIN_TYPE_MEMORY, KeyChainDAT::KeyChainType::Memory }
};

//******************************************************************************
KeyChainDAT::KeyChainDAT(const OP_NodeInfo* info)
: BaseDAT(info)
, keyChainType_(KeyChainType::File)
, faceDat_("face")
{
}

KeyChainDAT::~KeyChainDAT()
{
}

void
KeyChainDAT::execute(DAT_Output* output,
							const OP_Inputs* inputs,
							void* reserved)
{
    BaseDAT::execute(output, inputs, reserved);
}

int32_t
KeyChainDAT::getNumInfoCHOPChans(void* reserved1)
{
    return BaseDAT::getNumInfoCHOPChans(reserved1);
}

void
KeyChainDAT::getInfoCHOPChan(int32_t index,
									OP_InfoCHOPChan* chan, void* reserved1)
{
    BaseDAT::getInfoCHOPChan(index, chan, reserved1);
}

bool
KeyChainDAT::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
    return BaseDAT::getInfoDATSize(infoSize, reserved1);
}

void
KeyChainDAT::getInfoDATEntries(int32_t index,
									int32_t nEntries,
									OP_InfoDATEntries* entries,
									void* reserved1)
{
    BaseDAT::getInfoDATEntries(index, nEntries, entries, reserved1);
}

void
KeyChainDAT::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
    BaseDAT::setupParameters(manager, reserved1);
    
    static const char *names[PAR_KEYCHAIN_MENU_SIZE] = {PAR_KEYCHAIN_TYPE_FILE, PAR_KEYCHAIN_TYPE_SYSTEM, PAR_KEYCHAIN_TYPE_MEMORY};
    static const char *labels[PAR_KEYCHAIN_MENU_SIZE] = {PAR_KEYCHAIN_TYPE_FILE_LABEL, PAR_KEYCHAIN_TYPE_SYSTEM_LABEL, PAR_KEYCHAIN_TYPE_MEMORY_LABEL};
    
    appendPar<OP_StringParameter>
    (manager, PAR_KEYCHAIN_MENU, PAR_KEYCHAIN_MENU_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         string defaultValue;
         for (auto p : KeyChainTypeMap)
             if (p.second == keyChainType_)
             {
                 defaultValue = p.first;
                 break;
             }
         
         p.defaultValue = defaultValue.c_str();
         
         return manager->appendMenu(p, PAR_KEYCHAIN_MENU_SIZE, names, labels);
     });
    
    appendPar<OP_StringParameter>
    (manager, PAR_FACE_DAT, PAR_FACE_DAT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
     });
}

void
KeyChainDAT::pulsePressed(const char* name, void* reserved1)
{
}

void
KeyChainDAT::initPulsed()
{
    // reinit
}

void
KeyChainDAT::checkInputs(set<string> &paramNames, DAT_Output *, const OP_Inputs *inputs, void *)
{
    {
        string s = inputs->getParString(PAR_KEYCHAIN_MENU);
        KeyChainType t = KeyChainTypeMap.at(s);
    
        if (keyChainType_ != t)
        {
            t = keyChainType_;
            paramNames.insert(PAR_KEYCHAIN_MENU);
        }
    }
    
    {
        string s = inputs->getParString(PAR_FACE_DAT);
        
        if (faceDat_ != s)
        {
            faceDat_ = s;
            paramNames.insert(PAR_FACE_DAT);
        }
    }
}


void
KeyChainDAT::paramsUpdated(const set<string> &updatedParams)
{
    if (updatedParams.find(PAR_KEYCHAIN_MENU) != updatedParams.end())
    {
        // re-init keychain
    }
    
    if (updatedParams.find(PAR_FACE_DAT) != updatedParams.end())
    {
        // re-configure face
    }
}
