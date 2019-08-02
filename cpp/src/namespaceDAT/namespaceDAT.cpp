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

#include "namespaceDAT.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <array>


#include <cnl-cpp/namespace.hpp>
#include <cnl-cpp/segmented-object-handler.hpp>
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>
#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

#include "faceDAT.h"
#include "keyChainDAT.h"
#include "key-chain-manager.hpp"
#include "face-processor.hpp"

#define PAR_PREFIX "Prefix"
#define PAR_PREFIX_LABEL "Prefix"
#define PAR_FACEDAT "Facedat"
#define PAR_FACEDAT_LABEL "Face DAT"
#define PAR_KEYCHAINDAT "Keychaindat"
#define PAR_KEYCHAINDAT_LABEL "KeyChain DAT"
#define PAR_FRESHNESS "Freshness"
#define PAR_FRESHNESS_LABEL "Freshness"
#define PAR_HANDLER_TYPE "Handlertype"
#define PAR_HANDLER_TYPE_LABEL "Handler"
#define PAR_TOP_INPUT "Topinput"
#define PAR_TOP_INPUT_LABEL "Payload TOP"
#define PAR_OUTPUT "Output"
#define PAR_OUTPUT_LABEL "Raw Output"

#define PAR_HANDLER_NONE "Handlernone"
#define PAR_HANDLER_NONE_LABEL "None"
#define PAR_HANDLER_SEGMENTED "Handlersegmented"
#define PAR_HANDLER_SEGMENTED_LABEL "Segmented"
#define PAR_HANDLER_GOBJ "Handlergobj"
#define PAR_HANDLER_GOBJ_LABEL "Generalized Object"
#define PAR_HANDLER_GOSTREAM "Handlergobjstream"
#define PAR_HANDLER_GOSTREAM_LABEL "Generalized Object Stream"
#define PAR_OBJECT_NEEDED "Objectneeded"
#define PAR_OBJECT_NEEDED_LABEL "Object Needed"

using namespace std;
using namespace std::placeholders;
using namespace touch_ndn;
using namespace ndn;
using namespace cnl_cpp;

//******************************************************************************
const map<string, NamespaceDAT::HandlerType> HandlerTypeMap = {
    { PAR_HANDLER_NONE, NamespaceDAT::HandlerType::None },
    { PAR_HANDLER_SEGMENTED, NamespaceDAT::HandlerType::Segmented },
    { PAR_HANDLER_GOBJ, NamespaceDAT::HandlerType::GObj },
    { PAR_HANDLER_GOSTREAM, NamespaceDAT::HandlerType::GObjStream },
};

extern "C"
{

DLLEXPORT
void
FillDATPluginInfo(DAT_PluginInfo *info)
{
	info->apiVersion = DATCPlusPlusAPIVersion;
	info->customOPInfo.opType->setString("Ndnnamespace");
	info->customOPInfo.opLabel->setString("Namespace DAT");
	info->customOPInfo.opIcon->setString("NDT");
	info->customOPInfo.authorName->setString("Peter Gusev");
	info->customOPInfo.authorEmail->setString("peter@remap.ucla.edu");
	info->customOPInfo.minInputs = 0;
	info->customOPInfo.maxInputs = 1;
}

DLLEXPORT
DAT_CPlusPlusBase*
CreateDATInstance(const OP_NodeInfo* info)
{
	return new NamespaceDAT(info);
}

DLLEXPORT
void
DestroyDATInstance(DAT_CPlusPlusBase* instance)
{
	delete (    NamespaceDAT*)instance;
}

};

NamespaceDAT::NamespaceDAT(const OP_NodeInfo* info)
: BaseDAT(info)
, freshness_(4000)
, handlerType_(HandlerType::GObj)
, faceDatOp_(nullptr)
, keyChainDatOp_(nullptr)
, rawOutput_(true)
, gobjMetaInfoRows_(make_shared<MetaInfoRows>())
{
}

NamespaceDAT::~NamespaceDAT()
{
}

void
NamespaceDAT::getGeneralInfo(DAT_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1)
{
    BaseDAT::getGeneralInfo(ginfo, inputs, reserved1);
    ginfo->cookEveryFrameIfAsked = true;
}

void
NamespaceDAT::execute(DAT_Output* output,
							const OP_Inputs* inputs,
							void* reserved)
{
    BaseDAT::execute(output, inputs, reserved);
    
    if (!namespace_)
        initNamespace(output, inputs, reserved);
    
    if (namespace_ && namespace_->getFace_())
    {
        bool isFetching = !(inputs->getNumInputs() || payloadTop_.size());
        switch (namespace_->getState()) {
            case NamespaceState_NAME_EXISTS:
            {
                if (isFetching)
                    runFetch(output, inputs, reserved);
                else
                    runPublish(output, inputs, reserved);
            }
                break;
            case NamespaceState_OBJECT_READY:
            {
                setOutput(output, inputs, reserved);
            }
                break;
            default:
                // do nothing
                break;
        }
    }
    else
        output->setText("");
}

void
NamespaceDAT::setOutput(DAT_Output *output, const OP_Inputs* inputs, void* reserved)
{
    if (namespace_ && namespace_->getState() == NamespaceState_OBJECT_READY)
        switch (handlerType_)
        {
            case HandlerType::None: // fallthrough
            case HandlerType::GObj:
                if (rawOutput_)
                    output->setText(namespace_->getBlobObject().toRawStr().c_str());
                else
                    output->setText(BaseDAT::toBase64(namespace_->getBlobObject()).c_str());
                break;
            default:
                break;
        } // switch
    else
        output->setText("");
}

bool
NamespaceDAT::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
    BaseDAT::getInfoDATSize(infoSize, reserved1);
    int packetsRow = (namespace_ && namespace_->getState() == NamespaceState_OBJECT_READY ? 1 : 0);
    int nDefaultRows = 2 + packetsRow;
    
    infoSize->rows += nDefaultRows + gobjMetaInfoRows_->size();
    infoSize->cols = 2;
    infoSize->byColumn = false;
    
    return true;
}

void
NamespaceDAT::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries,
                                void* reserved1)
{
    int packetsRow = (namespace_ && namespace_->getState() == NamespaceState_OBJECT_READY ? 1 : 0);
    int nDefaultRows = 2 + packetsRow;
    int nRows = nDefaultRows + (int)gobjMetaInfoRows_->size();
    if (index < nRows)
    {
        static const map<NamespaceState, string> NamespaceStateMap = {
            { NamespaceState_NAME_EXISTS, "NAME_EXISTS" },
            { NamespaceState_INTEREST_EXPRESSED, "INTEREST_EXPRESSED" },
            { NamespaceState_INTEREST_TIMEOUT, "INTEREST_TIMEOUT" },
            { NamespaceState_INTEREST_NETWORK_NACK, "INTEREST_NETWORK_NACK" },
            { NamespaceState_DATA_RECEIVED, "DATA_RECEIVED" },
            { NamespaceState_DESERIALIZING, "DESERIALIZING" },
            { NamespaceState_DECRYPTING, "DECRYPTING" },
            { NamespaceState_DECRYPTION_ERROR, "DECRYPTION_ERROR" },
            { NamespaceState_PRODUCING_OBJECT, "PRODUCING_OBJECT" },
            { NamespaceState_SERIALIZING, "SERIALIZING" },
            { NamespaceState_ENCRYPTING, "ENCRYPTING" },
            { NamespaceState_ENCRYPTION_ERROR, "ENCRYPTION_ERROR" },
            { NamespaceState_SIGNING, "SIGNING" },
            { NamespaceState_SIGNING_ERROR, "SIGNING_ERROR" },
            { NamespaceState_OBJECT_READY, "OBJECT_READY" },
            { NamespaceState_OBJECT_READY_BUT_STALE, "OBJECT_READY_BUT_STALE" }
        };
        
        switch (index) {
            case 0:
                entries->values[0]->setString("Full Name");
                entries->values[1]->setString( namespace_ ? namespace_->getName().toUri().c_str() : "" );
                break;
            case 1:
                entries->values[0]->setString("State");
                entries->values[1]->setString( namespace_ ? NamespaceStateMap.at(namespace_->getState()).c_str() : "n/a" );
                break;
            case 2:
            {
                vector<shared_ptr<Data>> dataList;
                namespace_->getAllData(dataList);
                entries->values[0]->setString("Total Packets");
                entries->values[1]->setString(to_string(dataList.size()).c_str());
            }
                break;
            default:
                int i = index - nDefaultRows;
                entries->values[0]->setString((*gobjMetaInfoRows_)[i].first.c_str());
                entries->values[1]->setString((*gobjMetaInfoRows_)[i].second.c_str());
                break;
        }
    }
    else
        BaseDAT::getInfoDATEntries(index-2, nEntries, entries, reserved1);
}


void
NamespaceDAT::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
    BaseDAT::setupParameters(manager, reserved1);
    
    appendPar<OP_StringParameter>
    (manager, PAR_PREFIX, PAR_PREFIX_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendString(p);
     });
    
    appendPar<OP_StringParameter>
    (manager, PAR_FACEDAT, PAR_FACEDAT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
     });
    appendPar<OP_StringParameter>
    (manager, PAR_KEYCHAINDAT, PAR_KEYCHAINDAT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
     });
    appendPar<OP_NumericParameter>
    (manager, PAR_FRESHNESS, PAR_FRESHNESS_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.minValues[0] = 0;
         p.maxValues[0] = 24*3600*1000;
         p.minSliders[0] = p.minValues[0];
         p.maxSliders[0] = p.maxValues[0];
         p.defaultValues[0] = freshness_;
         return manager->appendInt(p);
     });
    
#define PAR_HANDLER_MENU_SIZE 4
    static const char *names[PAR_HANDLER_MENU_SIZE] = {
        PAR_HANDLER_NONE,
        PAR_HANDLER_SEGMENTED,
        PAR_HANDLER_GOBJ,
        PAR_HANDLER_GOSTREAM
    };
    static const char *labels[PAR_HANDLER_MENU_SIZE] = {
        PAR_HANDLER_NONE_LABEL,
        PAR_HANDLER_SEGMENTED_LABEL,
        PAR_HANDLER_GOBJ_LABEL,
        PAR_HANDLER_GOSTREAM_LABEL
    };
    
    appendPar<OP_StringParameter>
    (manager, PAR_HANDLER_TYPE, PAR_HANDLER_TYPE_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         for (auto it:HandlerTypeMap)
             if (it.second == handlerType_)
             {
                 p.defaultValue = it.first.c_str();
                 break;
             }
         return manager->appendMenu(p, PAR_HANDLER_MENU_SIZE, names, labels);
     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_OBJECT_NEEDED, PAR_OBJECT_NEEDED_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         return manager->appendPulse(p);
     });
    
    appendPar<OP_StringParameter>
    (manager, PAR_TOP_INPUT, PAR_TOP_INPUT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_OUTPUT, PAR_OUTPUT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.defaultValues[0] = rawOutput_;
         return manager->appendToggle(p);
     });
}

void
NamespaceDAT::pulsePressed(const char* name, void* reserved1)
{
    if (string(name) == PAR_OBJECT_NEEDED)
    {
        if (namespace_ && namespace_->getFace_())
            runFetch(nullptr, nullptr, nullptr);
    }
    else
        BaseDAT::pulsePressed(name, reserved1);
}

void
NamespaceDAT::initPulsed()
{
    releaseNamespace(nullptr, nullptr, nullptr);
}

void
NamespaceDAT::onOpUpdate(OP_Common *op, const std::string &event)
{
    if (faceDatOp_ == op)
        unpairFaceDatOp(nullptr, nullptr, nullptr);
    if (keyChainDatOp_ == op)
        unpairKeyChainDatOp(nullptr, nullptr, nullptr);
}

void
NamespaceDAT::checkParams(DAT_Output*, const OP_Inputs* inputs, void* reserved)
{
    updateIfNew<string>
    (PAR_PREFIX, prefix_, inputs->getParString(PAR_PREFIX));
    
    updateIfNew<string>
    (PAR_FACEDAT, faceDat_, getCanonical(inputs->getParString(PAR_FACEDAT)),
     [&](string& p){
         return (p != faceDat_) || (faceDatOp_ == nullptr && p.size());
     });
    
    updateIfNew<string>
    (PAR_KEYCHAINDAT, keyChainDat_, getCanonical(inputs->getParString(PAR_KEYCHAINDAT)));

    updateIfNew<uint32_t>
    (PAR_FRESHNESS, freshness_, inputs->getParInt(PAR_FRESHNESS));
    
    updateIfNew<string>
    (PAR_TOP_INPUT, payloadTop_, getCanonical(inputs->getParString(PAR_TOP_INPUT)));
    
    updateIfNew<HandlerType>
    (PAR_HANDLER_TYPE, handlerType_, HandlerTypeMap.at(inputs->getParString(PAR_HANDLER_TYPE)));
    
    updateIfNew<bool>
    (PAR_OUTPUT, rawOutput_, (bool)inputs->getParInt(PAR_OUTPUT));
}

void
NamespaceDAT::paramsUpdated()
{
    runIfUpdated(PAR_PREFIX, [this](){
        dispatchOnExecute(bind(&NamespaceDAT::initNamespace, this, _1, _2, _3));
    });
    
    runIfUpdated(PAR_FACEDAT, [this](){
        dispatchOnExecute(bind(&NamespaceDAT::pairFaceDatOp, this, _1, _2, _3));
    });
    
    runIfUpdated(PAR_KEYCHAINDAT, [this](){
        dispatchOnExecute(bind(&NamespaceDAT::pairKeyChainDatOp, this, _1, _2, _3));
    });
    
    runIfUpdated(PAR_OUTPUT, [this](){
        if (namespace_ && namespace_->getState() == NamespaceState_OBJECT_READY)
            dispatchOnExecute(bind(&NamespaceDAT::setOutput, this, _1, _2, _3));
    });
}

void
NamespaceDAT::initNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved)
{
    releaseNamespace(output, inputs, reserved);
    setOutput(output, inputs, reserved);
    
    if (!faceDatOp_)
    {
        setError("FaceDAT is not set");
        return;
    }
    
    bool isProducer = (inputs->getNumInputs() || payloadTop_.size());
    KeyChain *keyChain = 0;
    
    if (isProducer)
    {
        if (!keyChainDatOp_)
        {
            setError("KeyChainDAT is not set");
            return;
        }
        keyChain = keyChainDatOp_->getKeyChainManager()->instanceKeyChain().get();
    }
        
    if (faceDatOp_->getFaceProcessor())
    {
        if (!prefix_.size())
        {
            setError("Namespace requires a prefix");
            return;
        }
        
        clearError();
        
        namespace_ = make_shared<Namespace>(prefix_, keyChain);
        if (isProducer)
        {
            MetaInfo metaInfo;
            metaInfo.setFreshnessPeriod(freshness_);
            namespace_->setNewDataMetaInfo(metaInfo);
        }
        
        shared_ptr<Namespace> nmspc = namespace_;
        faceDatOp_->getFaceProcessor()->dispatchSynchronized([nmspc](shared_ptr<Face> f){
            nmspc->setFace(f.get());
        });
    }
}

void
NamespaceDAT::releaseNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved)
{
    if (namespace_.get())
    {
        namespace_.reset();
    }
}

void
NamespaceDAT::pairFaceDatOp(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    int err = 1;
    void *faceDatOp;
    
    unpairFaceDatOp(output, inputs, reserved);
    
    if (faceDat_ == "")
    {
        err = 0;
    }
    else if ((faceDatOp = retrieveOp(faceDat_)))
    {
        faceDatOp_ = (FaceDAT*)faceDatOp;
        err = (faceDatOp_ ? 0 : 1);
        
        if (faceDatOp_)
            faceDatOp_->subscribe(this);
    }
    
    if (err)
        setError("Failed to find TouchNDN operator %s", faceDat_.c_str());
}

void
NamespaceDAT::unpairFaceDatOp(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    if (faceDatOp_)
    {
        releaseNamespace(output, inputs, reserved);
        faceDatOp_->unsubscribe(this);
        faceDatOp_ = nullptr;
    }
}

void
NamespaceDAT::pairKeyChainDatOp(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    int err = 1;
    void *keyChainDatOp;
    
    unpairKeyChainDatOp(output, inputs, reserved);
    
    if (keyChainDat_ == "")
    {
        err = 0;
    }
    else if ((keyChainDatOp = retrieveOp(keyChainDat_)))
    {
        keyChainDatOp_ = (KeyChainDAT*)keyChainDatOp;
        err = (keyChainDatOp_ ? 0 : 1);
        
        if (keyChainDatOp_)
            keyChainDatOp_->subscribe(this);
    }
    
    if (err)
        setError("Failed to find TouchNDN operator %s", keyChainDat_.c_str());
}


void
NamespaceDAT::unpairKeyChainDatOp(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    if (keyChainDatOp_)
    {
        if (!inputs || inputs->getNumInputs())
            releaseNamespace(output, inputs, reserved);
        
        keyChainDatOp_->unsubscribe(this);
        keyChainDatOp_ = nullptr;
    }
}

void
NamespaceDAT::runPublish(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    const OP_DATInput *datInput = inputs->getInputDAT(0);
    const char *str = datInput->getCell(0, 0);
    std::string payload(str);
    
    try {
        
        switch (handlerType_)
        {
            case HandlerType::Segmented: // fallthrough
            {
                SegmentedObjectHandler handler(namespace_.get());
            }
            case HandlerType::None:
            {
                namespace_->serializeObject(make_shared<BlobObject>(Blob::fromRawStr(payload)));
            }
                break;
            case HandlerType::GObj:
            {
                string contentType = (datInput->isTable && datInput->numRows > 1 ? datInput->getCell(1, 0) : "text/html");
                GeneralizedObjectHandler().setObject(*namespace_, Blob::fromRawStr(payload), contentType);
            }
                break;
            case HandlerType::GObjStream:
                // tbd
                break;
            default:
                break;
        }
        
        clearError();
    }
    catch (std::runtime_error &e)
    {
        setError(e.what());
    }
}

void
NamespaceDAT::runFetch(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    switch (handlerType_)
    {
        case HandlerType::None :
            namespace_->objectNeeded();
            break;
        case HandlerType::Segmented:
            SegmentedObjectHandler(namespace_.get()).objectNeeded();
            break;
        case HandlerType::GObj:
        {
            gobjMetaInfoRows_->clear();
            shared_ptr<MetaInfoRows> metaInfoRows = gobjMetaInfoRows_;
            bool rawOutput = rawOutput_;
            GeneralizedObjectHandler(namespace_.get(), [metaInfoRows, rawOutput](const shared_ptr<ContentMetaInfoObject> &contentMetaInfo, Namespace&){
                metaInfoRows->push_back(pair<string, string>("ContentType", contentMetaInfo->getContentType()));
                metaInfoRows->push_back(pair<string, string>("Timestamp", to_string(contentMetaInfo->getTimestamp())));
                
                string other = (rawOutput ? contentMetaInfo->getOther().toRawStr() : BaseDAT::toBase64(contentMetaInfo->getOther()));
                metaInfoRows->push_back(pair<string, string>("Other", other));
            }).objectNeeded();
        }
            break;
        case HandlerType::GObjStream:
            
            break;
        default:
            setError("Unsupported handler type");
            break;
    }
}
