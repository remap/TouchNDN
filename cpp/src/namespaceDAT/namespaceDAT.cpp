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
#include <fstream>

#include <cnl-cpp/namespace.hpp>
#include <cnl-cpp/segmented-object-handler.hpp>
#include <cnl-cpp/generalized-object/generalized-object-handler.hpp>
#include <cnl-cpp/generalized-object/generalized-object-stream-handler.hpp>

#include "common/contrib/json11/json11.hpp"
#include "faceDAT.h"
#include "keyChainDAT.h"
#include "payloadTOP.hpp"
#include "key-chain-manager.hpp"
#include "face-processor.hpp"

#define MODULE_LOGGER "namespaceDAT"

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
#define PAR_INPUT "Input"
#define PAR_INPUT_LABEL "Payload Input"
#define PAR_OUTPUT "Output"
#define PAR_OUTPUT_LABEL "Payload Output"
#define PAR_RAWOUTPUT "Rawoutput"
#define PAR_RAWOUTPUT_LABEL "Raw Output"

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

const map<NamespaceState, string> NamespaceStateMap = {
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

namespace touch_ndn {
    shared_ptr<helpers::logger> getModuleLogger()
    {
        return getLogger(MODULE_LOGGER);
    }
}

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
, freshness_(16000)
, handlerType_(HandlerType::GObj)
, faceDatOp_(nullptr)
, keyChainDatOp_(nullptr)
, rawOutput_(true)
, namespaceInfoRows_(make_shared<NamespaceInfoRows>())
, payloadInput_("")
, payloadOutput_("")
, onStateChangedCallbackId_(0)
, mustBeFresh_(true)
, namespaceState_(make_shared<atomic<NamespaceState>>())
, namespaceObject_(make_shared<shared_ptr<cnl_cpp::Object>>())
, gobjContentMetaInfo_(make_shared<shared_ptr<ContentMetaInfoObject>>())
{
    OPLOG_DEBUG("Created NamespaceDAT");
}

NamespaceDAT::~NamespaceDAT()
{
    releaseNamespace(nullptr, nullptr, nullptr);
    OPLOG_DEBUG("Released NamespaceDAT");
}

void
NamespaceDAT::getGeneralInfo(DAT_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1)
{
    ginfo->cookEveryFrame = false;
    // has to be true, because otherwise the OP isn't cooking enough to run its logic
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
        bool isFetching = !isProducer(inputs);
        switch (namespaceState_->load()) {
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
                // do nothing
            }
                break;
            default:
                // do nothing
                break;
        }
    }

    setOutput(output, inputs, reserved);
}

void
NamespaceDAT::setOutput(DAT_Output *output, const OP_Inputs* inputs, void* reserved)
{
    if (namespaceState_->load() == NamespaceState_OBJECT_READY)
    {
        switch (handlerType_)
        {
            case HandlerType::None: // fallthrough
            case HandlerType::Segmented: // fallthrough
            case HandlerType::GObj:
                {
                    assert(*namespaceObject_);
                    shared_ptr<BlobObject> b = dynamic_pointer_cast<BlobObject>(*namespaceObject_);
                    if (b)
                    {
                        clearError();
                        if (rawOutput_)
                            output->setText(b->toRawStr().c_str());
                        else
                            output->setText(BaseDAT::toBase64(b->getBlob()).c_str());
                    }
                    else
                    {
                        setError("Failed to process received object");
                        OPLOG_ERROR("Failed to cast received Object to BlobObject");
                    }
                }
                break;
            default:
                break;
        } // switch
        
        // for consumer -- check if need to save to a TOP or a file
        storeOutput(output, inputs, reserved);
    }
    else
        output->setText("");
}

void
NamespaceDAT::storeOutput(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    bool storePayload = !isProducer(inputs) && payloadOutput_.size() && !payloadStored_;
    
    if (namespaceState_->load() == NamespaceState_OBJECT_READY &&
        storePayload)
    {
        shared_ptr<BlobObject> blobObject = dynamic_pointer_cast<BlobObject>(*namespaceObject_);
        if (!blobObject)
        {
            OPLOG_ERROR("Failed to cast received Object to BlobObject");
            return;
        }
        
        if (retrieveOp(getCanonical(payloadOutput_)))
        {
            // save to TOP
            PayloadTOP *payloadTOP = (PayloadTOP*)retrieveOp(getCanonical(payloadOutput_));
            assert(payloadTOP);
            if (*gobjContentMetaInfo_)
            {
                string jsonErr;
                json11::Json json = json11::Json::parse((*gobjContentMetaInfo_)->getOther().toRawStr(), jsonErr);
                if (jsonErr.size() == 0)
                {
                    int w = json["width"].int_value();
                    int h = json["height"].int_value();
//                    int sz = json["size"].int_value();
                    
                    clearError();
                    payloadTOP->setBuffer(*blobObject->getBlob(), w, h);
                    payloadStored_ = true;
                }
                else
                {
                    setError("Error processing received object");
                    OPLOG_ERROR("Received ContentMetaInfo is a bad JSON");
                }
            }
        }
        else // save to a file
        {
            ofstream file(payloadOutput_, ios_base::out | ios_base::binary);
            if (file)
            {
                file.write((const char*)blobObject->getBlob().buf(), blobObject->getBlob().size());
                if (!file)
                {
                    setError("Unable to write to file %s", payloadOutput_.c_str());
                    OPLOG_ERROR("Failed to write to file {}", payloadOutput_);
                }
                else
                {
                    clearError();
                    payloadStored_ = true;
                }
            }
            else
            {
                setError("Unable to open file %s", payloadOutput_.c_str());
                OPLOG_ERROR("Failed to open file {}", payloadOutput_);
            }
        }
    }
}

shared_ptr<ndn::Blob>
NamespaceDAT::getPayload(const OP_Inputs *inputs, string& contentType,
                         shared_ptr<Blob>& other) const
{
    contentType = "text/html";

    // give priority to inputs
    if (inputs->getNumInputs())
    {
        const OP_DATInput *datInput = inputs->getInputDAT(0);
        if (datInput->numCols == 0 || datInput->numRows == 0)
            // got nothing
            return shared_ptr<Blob>();
    
        contentType = (datInput->isTable && datInput->numRows > 1 ? datInput->getCell(1, 0) : "text/html");
        return make_shared<Blob>(Blob::fromRawStr(datInput->getCell(0, 0)));
    }
    else
    {
        // payloadInput_ can either point to a file or a PayloadTOP
        // check TOP first
        if (retrieveOp(getCanonical(payloadInput_)))
        {
            // load payload from the TOP
            PayloadTOP *payloadTop = (PayloadTOP*)retrieveOp(getCanonical(payloadInput_));
            int size, w, h;
            shared_ptr<vector<uint8_t>> buffer = payloadTop->getBuffer(size, w, h);
            contentType = "image/x-bgra";
            
            json11::Json json = json11::Json::object {
                { "width", w },
                { "height", h },
                { "size", size }
            };
            other = make_shared<Blob>(Blob::fromRawStr(json.dump()));
            return make_shared<Blob>(buffer);
        }
        else // treat payloadInput_ as a path to a file
        {
            ifstream file(payloadInput_, ios_base::binary | ios_base::in | ios_base::ate);
            shared_ptr<Blob> b;
            if (file.is_open())
            {
                streamsize sz = file.tellg();
                shared_ptr<vector<uint8_t>> buf = make_shared<vector<uint8_t>>(sz);
                file.seekg(0);
                
                OPLOG_TRACE("Reading file {} of size {}", payloadInput_, sz);
                if (!file.read((char*)(buf->data()), sz))
                {
                    OPLOG_ERROR("Failed to read {} bytes from file {}", sz, payloadInput_);
                }
                else
                {
                    b = make_shared<Blob>(buf);
                    // TODO: set content type according to the extension
                    contentType = "text/html";
                }
            }
            else
                OPLOG_ERROR("Failed to open file {}", payloadInput_);
            
            return b;
        }
    }

    return shared_ptr<Blob>();
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
NamespaceDAT::initNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved)
{
    releaseNamespace(output, inputs, reserved);

    if (!faceDatOp_)
    {
        setError("FaceDAT is not set");
        return;
    }

    bool isProducer = this->isProducer(inputs);
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
        
        gobjContentMetaInfo_->reset();
        namespaceObject_->reset();
        namespaceState_->store(NamespaceState_NAME_EXISTS);
        namespace_ = make_shared<Namespace>(prefix_, keyChain);
        
        shared_ptr<atomic<NamespaceState>> namespaceState = namespaceState_;
        shared_ptr<shared_ptr<cnl_cpp::Object>> receivedObject = namespaceObject_;
        string outerNamespaceName = namespace_->getName().toUri();
        onStateChangedCallbackId_ = namespace_->addOnStateChanged([namespaceState, receivedObject, outerNamespaceName]
                                      (Namespace& n, Namespace& objectNamespace, NamespaceState state,
                                       uint64_t callbackId)
        {
            if (outerNamespaceName == objectNamespace.getName().toUri())
            {
                if (state == NamespaceState_OBJECT_READY)
                    *receivedObject = objectNamespace.getObject();
                namespaceState->store(state);
            }
        });
        
        if (isProducer)
        {
            MetaInfo metaInfo;
            metaInfo.setFreshnessPeriod(freshness_);
            namespace_->setNewDataMetaInfo(metaInfo);
            OPLOG_DEBUG("Created producer namespace {}", namespace_->getName().toUri());
        }
        else
            OPLOG_DEBUG("Created consumer namespace {}", namespace_->getName().toUri());

        namespaceInfoRows_->clear();
        namespaceInfoRows_->push_back(pair<string,string>("Type", isProducer ? "PRODUCER" : "CONSUMER"));
        prefixRegistered_ = make_shared<bool>(false);

        shared_ptr<helpers::logger> logger = logger_;
        shared_ptr<Namespace> nmspc = namespace_;
        shared_ptr<bool> prefixRegistered = prefixRegistered_;
        faceDatOp_->getFaceProcessor()->
            dispatchSynchronized([isProducer,nmspc,logger,prefixRegistered](shared_ptr<Face> f){
            logger->debug("Set face for namespace {}", nmspc->getName().toUri());
            
            if (isProducer)
                nmspc->setFace(f.get(),
                               [logger](const shared_ptr<const Name>& n){
                                   logger->error("Failed to register prefix {}", n->toUri());
                               },
                               [logger,prefixRegistered](const shared_ptr<const Name>& n,
                                                         uint64_t registeredPrefixId){
                                   *prefixRegistered = true;
                                   logger->debug("Registered prefix {}", n->toUri());
                               });
            else
                nmspc->setFace(f.get());
        });
    }
}

void
NamespaceDAT::releaseNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved)
{
    if (namespace_.get())
    {
        namespace_->removeCallback(onStateChangedCallbackId_);
        namespace_->setFace(nullptr);
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
        {
            faceDatOp_->subscribe(this);
            OPLOG_DEBUG("Paired FaceDAT {}", faceDatOp_->getFullPath());
        }
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
        OPLOG_DEBUG("Unpaired FaceDAT");
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
        {
            keyChainDatOp_->subscribe(this);
            OPLOG_DEBUG("Paired KeyChainDAT {}", keyChainDatOp_->getFullPath());
        }
    }
    
    if (err)
        setError("Failed to find TouchNDN operator %s", keyChainDat_.c_str());
}

void
NamespaceDAT::unpairKeyChainDatOp(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    if (keyChainDatOp_)
    {
        if (isProducer(inputs))
            releaseNamespace(output, inputs, reserved);
        
        keyChainDatOp_->unsubscribe(this);
        keyChainDatOp_ = nullptr;
        OPLOG_DEBUG("Unpaired KeyChainDAT");
    }
}

void
NamespaceDAT::runPublish(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    string contentType;
    shared_ptr<Blob> other;
    shared_ptr<Blob> payload = getPayload(inputs, contentType, other);
    
    if (!payload || payload->size() == 0)
    {
        setWarning("Can't read input. Not publishing");
        return;
    }
    
    try {
        
        switch (handlerType_)
        {
            case HandlerType::None:
            {
                namespace_->serializeObject(make_shared<BlobObject>(*payload));
            }
                break;
            case HandlerType::Segmented:
            {
                SegmentStreamHandler().setObject(*namespace_, *payload);
            }
                break;
            case HandlerType::GObj:
            {
                if (other)
                    GeneralizedObjectHandler().setObject(*namespace_, *payload, contentType, *other);
                else
                    GeneralizedObjectHandler().setObject(*namespace_, *payload, contentType);
            }
                break;
            case HandlerType::GObjStream:
                // tbd
                break;
            default:
                break;
        }
        
        clearWarning();
        clearError();
        
        OPLOG_DEBUG("Published data under {}", namespace_->getName().toUri());

        vector<shared_ptr<Data>> allData;
        namespace_->getAllData(allData);
        int idx = 0;
        for (auto d:allData)
        {
            namespaceInfoRows_->push_back(pair<string,string>("Packet "+to_string(idx++), d->getName().toUri()));
            OPLOG_TRACE("{}", d->getName().toUri());
        }

    }
    catch (std::runtime_error &e)
    {
        setError(e.what());
        OPLOG_ERROR("Error while publishing: {}", e.what());
    }
}

void
NamespaceDAT::runFetch(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    switch (handlerType_)
    {
        case HandlerType::None:
            namespace_->objectNeeded(mustBeFresh_);
            OPLOG_DEBUG("Data packet requested: {}", namespace_->getName().toUri());
            break;
        case HandlerType::Segmented:
            SegmentedObjectHandler(namespace_.get()).objectNeeded(mustBeFresh_);
            OPLOG_DEBUG("Segmented data requested {}", namespace_->getName().toUri());
            break;
        case HandlerType::GObj:
        {
            shared_ptr<shared_ptr<ContentMetaInfoObject>> gobjContentMetaInfo = gobjContentMetaInfo_;
            shared_ptr<NamespaceInfoRows> metaInfoRows = namespaceInfoRows_;
            bool rawOutput = rawOutput_;
            GeneralizedObjectHandler(namespace_.get(),
                                     [metaInfoRows, rawOutput, gobjContentMetaInfo]
                                     (const shared_ptr<ContentMetaInfoObject> &contentMetaInfo, Namespace &objectNamespace)
            {
                metaInfoRows->push_back(pair<string, string>("ContentType", contentMetaInfo->getContentType()));
                metaInfoRows->push_back(pair<string, string>("Timestamp", to_string(contentMetaInfo->getTimestamp())));
                
                string other = (rawOutput ? contentMetaInfo->getOther().toRawStr() : BaseDAT::toBase64(contentMetaInfo->getOther()));
                metaInfoRows->push_back(pair<string, string>("Other", other));
                *gobjContentMetaInfo = contentMetaInfo;
            }).objectNeeded(mustBeFresh_);
            OPLOG_DEBUG("Generalized Object requested {}", namespace_->getName().toUri());
        }
            break;
        case HandlerType::GObjStream:
            
            break;
        default:
            setError("Unsupported handler type");
            break;
    }

    payloadStored_ = false;
}

#define NDEFAULT_ROWS 3
bool
NamespaceDAT::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
    BaseDAT::getInfoDATSize(infoSize, reserved1);
    int packetsRow = (namespace_ && namespace_->getState() == NamespaceState_OBJECT_READY ? 1 : 0);
    int nDefaultRows = NDEFAULT_ROWS + packetsRow;
    
    infoSize->rows += nDefaultRows + namespaceInfoRows_->size();
    infoSize->cols = 2;
    infoSize->byColumn = false;
    
    return true;
}

void
NamespaceDAT::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries,
                                void* reserved1)
{
    int packetsRow = (namespace_ && namespace_->getState() == NamespaceState_OBJECT_READY ? 1 : 0);
    int nDefaultRows = NDEFAULT_ROWS + packetsRow;
    int nRows = nDefaultRows + (int)namespaceInfoRows_->size();
    if (index < nRows)
    {
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
                entries->values[0]->setString("Prefix Registered");
                entries->values[1]->setString(prefixRegistered_ && *prefixRegistered_ ? "true" : "false");
                break;
            case 3:
                if (packetsRow)
                {
                    vector<shared_ptr<Data>> dataList;
                    namespace_->getAllData(dataList);
                    entries->values[0]->setString("Total Packets");
                    entries->values[1]->setString(to_string(dataList.size()).c_str());
                    break;
                }// else -- fallthrough
            default:
                int i = index - nDefaultRows;
                entries->values[0]->setString((*namespaceInfoRows_)[i].first.c_str());
                entries->values[1]->setString((*namespaceInfoRows_)[i].second.c_str());
                break;
        }
    }
    else
        BaseDAT::getInfoDATEntries(index-2, nEntries, entries, reserved1);
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
    appendPar<OP_StringParameter>
    (manager, PAR_INPUT, PAR_INPUT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         p.defaultValue = payloadInput_.c_str();
         return manager->appendString(p);
     });
    appendPar<OP_StringParameter>
    (manager, PAR_OUTPUT, PAR_OUTPUT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         p.defaultValue = payloadOutput_.c_str();
         return manager->appendString(p);
     });
    
//    appendPar<OP_NumericParameter>
//    (manager, PAR_OBJECT_NEEDED, PAR_OBJECT_NEEDED_LABEL, PAR_PAGE_DEFAULT,
//     [&](OP_NumericParameter &p){
//         return manager->appendPulse(p);
//     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_RAWOUTPUT, PAR_RAWOUTPUT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.defaultValues[0] = rawOutput_;
         return manager->appendToggle(p);
     });
}

bool
NamespaceDAT::isProducer(const OP_Inputs* inputs)
{
    // namespace publishes object if
    // - keyChain is set AND
    // - there is an input OR payloadInput_ is not empty
    return keyChainDatOp_ && (inputs->getNumInputs() || payloadInput_.size());
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
    (PAR_KEYCHAINDAT, keyChainDat_, getCanonical(inputs->getParString(PAR_KEYCHAINDAT)),
     [&](string& p){
         return (p != keyChainDat_) || (keyChainDatOp_ == nullptr && p.size());
     });
    
    updateIfNew<uint32_t>
    (PAR_FRESHNESS, freshness_, inputs->getParInt(PAR_FRESHNESS));
    
    updateIfNew<HandlerType>
    (PAR_HANDLER_TYPE, handlerType_, HandlerTypeMap.at(inputs->getParString(PAR_HANDLER_TYPE)));
    
    updateIfNew<string>
    (PAR_INPUT, payloadInput_, inputs->getParString(PAR_INPUT));
    
    updateIfNew<string>
    (PAR_OUTPUT, payloadOutput_, inputs->getParString(PAR_OUTPUT));
    
    updateIfNew<bool>
    (PAR_RAWOUTPUT, rawOutput_, (bool)inputs->getParInt(PAR_RAWOUTPUT));
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
    
    runIfUpdated(PAR_RAWOUTPUT, [this](){
        if (namespace_ && namespaceState_->load() == NamespaceState_OBJECT_READY)
            dispatchOnExecute(bind(&NamespaceDAT::setOutput, this, _1, _2, _3));
    });
    
    runIfUpdated(PAR_OUTPUT, [this](){
        if (namespace_ && namespaceState_->load() == NamespaceState_OBJECT_READY)
            dispatchOnExecute(bind(&NamespaceDAT::storeOutput, this, _1, _2, _3));
    });
    
}
