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
#include <ndn-cpp/data.hpp>

#include "faceDAT.h"
#include "touchNDN-shared.hpp"
#include "key-chain-manager.hpp"
#include "foundation-helpers.h"

#define PAR_KEYCHAIN_MENU "Keychainmenu"
#define PAR_KEYCHAIN_MENU_LABEL "KeyChain Type"
#define PAR_KEYCHAIN_TYPE_SYSTEM "Keychainsystem"
#define PAR_KEYCHAIN_TYPE_FILE "Keychainfile"
#define PAR_KEYCHAIN_TYPE_EMBED "Keychainembed"
#define PAR_KEYCHAIN_TYPE_SYSTEM_LABEL "System"
#define PAR_KEYCHAIN_TYPE_FILE_LABEL "File"
#define PAR_KEYCHAIN_TYPE_EMBED_LABEL "Embedded"

using namespace std;
using namespace std::placeholders;
using namespace ndn;
using namespace touch_ndn;
using namespace touch_ndn::helpers;

//******************************************************************************
const map<string, KeyChainDAT::KeyChainType> KeyChainTypeMap = {
    { PAR_KEYCHAIN_TYPE_SYSTEM, KeyChainDAT::KeyChainType::System },
    { PAR_KEYCHAIN_TYPE_FILE, KeyChainDAT::KeyChainType::File },
    { PAR_KEYCHAIN_TYPE_EMBED, KeyChainDAT::KeyChainType::Embedded }
};

//******************************************************************************
KeyChainDAT::KeyChainDAT(const OP_NodeInfo* info)
: BaseDAT(info)
, keyChainType_(KeyChainType::Embedded)
{
    dispatchOnExecute(bind(&KeyChainDAT::initKeyChain, this, _1, _2, _3));
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
     BaseDAT::getInfoDATSize(infoSize, reserved1);
    
    if (keyChainManager_)
    {
        infoSize->rows += 4;
        infoSize->cols = 2;
    }
    infoSize->byColumn = false;
    
    return true;
}

void
KeyChainDAT::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries,
                               void* reserved1)
{
    if (index < 4)
    {
        switch (index) {
            case 0:
                entries->values[0]->setString("Signing Identity");
                entries->values[1]->setString(keyChainManager_->getSigningIdentity().c_str());
                break;
            case 1:
                entries->values[0]->setString("Signing Identity Certificate");
                entries->values[1]->setString(keyChainManager_->signingIdentityCertificate()->getName().toUri().c_str());
                break;
            case 2:
                entries->values[0]->setString("Instance Identity");
                entries->values[1]->setString(keyChainManager_->getInstanceIdentity().c_str());
                break;
            case 3:
                entries->values[0]->setString("Instance Identity Certificate");
                entries->values[1]->setString(keyChainManager_->instanceCertificate()->getName().toUri().c_str());
                break;
            default:
                break;
        }
    }
    else
        BaseDAT::getInfoDATEntries(index-4, nEntries, entries, reserved1);
}

void
KeyChainDAT::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
    BaseDAT::setupParameters(manager, reserved1);
    
#define PAR_KEYCHAIN_MENU_SIZE 2
    static const char *names[PAR_KEYCHAIN_MENU_SIZE] = {PAR_KEYCHAIN_TYPE_EMBED,
                                                        //PAR_KEYCHAIN_TYPE_FILE,
                                                        PAR_KEYCHAIN_TYPE_SYSTEM};
    static const char *labels[PAR_KEYCHAIN_MENU_SIZE] = {PAR_KEYCHAIN_TYPE_EMBED_LABEL,
                                                         //PAR_KEYCHAIN_TYPE_FILE_LABEL,
                                                         PAR_KEYCHAIN_TYPE_SYSTEM_LABEL};
    
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
KeyChainDAT::checkParams(DAT_Output *, const OP_Inputs *inputs, void *)
{
    updateIfNew<KeyChainType>
    (PAR_KEYCHAIN_MENU, keyChainType_, KeyChainTypeMap.at(inputs->getParString(PAR_KEYCHAIN_MENU)));
}

void
KeyChainDAT::paramsUpdated()
{
    runIfUpdated(PAR_KEYCHAIN_MENU, [this](){
        // re-init keychain
        dispatchOnExecute(bind(&KeyChainDAT::initKeyChain, this, _1, _2, _3));
    });
}

void
KeyChainDAT::initKeyChain(DAT_Output *, const OP_Inputs *, void *reserved)
{
    if (keyChainType_ == KeyChainType::Embedded)
    {
        string signingIdentity = "/touchdesigner";
        string policyFilePath = string(get_resources_path())+"/policy.conf";
        string keyChainPath = string(get_resources_path())+"/keychain";
        string instanceName = string(generate_uuid());
        
        keyChainManager_ = make_shared<KeyChainManager>(KeyChainManager::createKeyChain(keyChainPath),
                                                        signingIdentity,
                                                        instanceName,
                                                        policyFilePath,
                                                        6*3600);
    }
}

void
KeyChainDAT::onOpUpdate(OP_Common *op, const std::string &event)
{

}
