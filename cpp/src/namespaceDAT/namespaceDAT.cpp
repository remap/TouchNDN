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
#define PAR_GOBJ_VERSIONED "Gobjversioned"
#define PAR_GOBJ_VERSIONED_LABEL "Versioned"

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

class NamespaceDAT::Impl : public enable_shared_from_this<NamespaceDAT::Impl> {
public:
    typedef struct _ObjectReadyPayload {
        // be careful with this raw pointer...
        cnl_cpp::Namespace* objectNamespace_;
        std::shared_ptr<cnl_cpp::Object> object_;
        std::shared_ptr<cnl_cpp::ContentMetaInfoObject> contentMetaInfo_;
    } ObjectReadyPayload;
    class PayloadData {
    public:
        static shared_ptr<PayloadData> fromDatInputData(shared_ptr<DatInputData> datInputData)
        {
            if (datInputData->handlerType_ == HandlerType::GObj ||
                datInputData->handlerType_ == HandlerType::GObjStream)
                return make_shared<GObjPayloadData>(datInputData);
            return make_shared<PayloadData>(datInputData);
        }
        
        PayloadData(shared_ptr<DatInputData> datInputData)
        {
            metaInfo_ = *datInputData->metaInfo_;
            payload_ = datInputData->payload_;
        }
        virtual ~PayloadData(){}
        
        MetaInfo metaInfo_;
        shared_ptr<Blob> payload_;
    };
    
    class GObjPayloadData : public PayloadData {
    public:
        GObjPayloadData(shared_ptr<DatInputData> datInputData)
        : PayloadData(datInputData)
        {
            assert((datInputData->handlerType_ == HandlerType::GObj ||
                    datInputData->handlerType_ == HandlerType::GObjStream));
            
            if (datInputData->inputFile_.size())
            {
                payload_ = NamespaceDAT::readFile(datInputData->inputFile_, contentType_, other_);
            }
            else
            {
                contentType_ = datInputData->contentType_;
                other_ = datInputData->other_;
            }
        }
        
        string contentType_;
        shared_ptr<Blob> other_;
    };
    
    shared_ptr<helpers::logger> logger_;
    HandlerType handlerType_;
    shared_ptr<Namespace> namespace_;
    vector<uint64_t> registeredCallbacks_;
    bool prefixRegistered_;
    ObjectReadyPayload objectReadyPayload_;
    
    Impl(shared_ptr<helpers::logger> &l) :
    handlerType_(HandlerType::GObj)
    , prefixRegistered_(false)
    , logger_(l) {}
    ~Impl(){}
    
    bool getIsObjectReady() const
    {
        return namespace_ && namespace_->getState() == NamespaceState_OBJECT_READY;
    }
    
    void initNamespace(string prefix, KeyChain *keyChain)
    {
        objectReadyPayload_ = ObjectReadyPayload();
        
        namespace_ = make_shared<Namespace>(prefix, keyChain);
        shared_ptr<Impl> me = shared_from_this();
        
        uint64_t cbId = namespace_->addOnStateChanged(
        [this, me](Namespace& n, Namespace& on, NamespaceState state, uint64_t cbId)
        {
            if (on.getName() == namespace_->getName())
            {
                // ???
            }
        });
        
        registeredCallbacks_.push_back(cbId);
    }
    
    void releaseNamespace()
    {
        if (namespace_)
        {
            for (auto cbId:registeredCallbacks_)
                namespace_->removeCallback(cbId);
            namespace_->setFace(nullptr);
            namespace_.reset();
        }
    }
    
    void setFace(shared_ptr<helpers::FaceProcessor> faceProcessor,
                 bool registerPrefix = false)
    {
        shared_ptr<Impl> me = shared_from_this();
        faceProcessor->dispatchSynchronized([this, me, registerPrefix](shared_ptr<Face> f)
        {
            logger_->debug("Set face for namespace {}", namespace_->getName().toUri());
            
            if (registerPrefix)
                namespace_->setFace(f.get(),
                               [me](const shared_ptr<const Name>& n){
                                   me->logger_->error("Failed to register prefix {}", n->toUri());
                               },
                               [me](const shared_ptr<const Name>& n,
                                                         uint64_t registeredPrefixId){
                                   me->prefixRegistered_ = true;
                                   me->logger_->debug("Registered prefix {}", n->toUri());
                               });
            else
                namespace_->setFace(f.get());
        });
    }
    
    void produceOnRequest(function<shared_ptr<PayloadData>()> getPayloadData,
                          bool versioned = false)
    {
        shared_ptr<Impl> me = shared_from_this();
        uint64_t cbId =
        namespace_->addOnObjectNeeded([this,me,versioned,getPayloadData]
                                      (Namespace& n, Namespace& neededNamespace, uint64_t)
        {
            return produceNow(*namespace_, getPayloadData(), versioned);
        });
        registeredCallbacks_.push_back(cbId);
    }
    
    bool produceNow(Namespace &n, shared_ptr<PayloadData> payloadData, bool versioned = false)
    {
        objectReadyPayload_.contentMetaInfo_.reset();
        objectReadyPayload_.object_.reset();
        objectReadyPayload_.objectNamespace_ = nullptr;
        
        Namespace &objectNamespace = versioned ?
            n[Name::Component::fromVersion((uint64_t)ndn_getNowMilliseconds())] : n;
        objectNamespace.setNewDataMetaInfo(payloadData->metaInfo_);
        
        try {
            switch (handlerType_)
            {
                case HandlerType::None:
                {
                    objectNamespace.serializeObject(make_shared<BlobObject>(*payloadData->payload_));
                }
                    break;
                case HandlerType::Segmented:
                {
                    SegmentStreamHandler().setObject(objectNamespace, *payloadData->payload_);
                }
                    break;
                case HandlerType::GObj:
                {
                    shared_ptr<GObjPayloadData> pd = dynamic_pointer_cast<GObjPayloadData>(payloadData);
                    assert(pd);
                    if (pd->other_)
                        GeneralizedObjectHandler().setObject(objectNamespace, *pd->payload_, pd->contentType_, *pd->other_);
                    else
                        GeneralizedObjectHandler().setObject(objectNamespace, *pd->payload_, pd->contentType_);
                    
                    ndntools::ContentMetaInfo cMetaInfo;
                    cMetaInfo.wireDecode(*objectNamespace[Name::Component("_meta")].getBlobObject());
                    objectReadyPayload_.contentMetaInfo_ = make_shared<ContentMetaInfoObject>(cMetaInfo);
                }
                    break;
                case HandlerType::GObjStream:
                    // tbd
                    break;
                default:
                    break;
            }
            
            //        clearWarning();
            //        clearError();
            objectReadyPayload_.objectNamespace_ = &objectNamespace;
            objectReadyPayload_.object_ = objectNamespace.getObject();
            logger_->debug("Published data under {}", n.getName().toUri());
            
            return true;
        }
        catch (std::runtime_error &e)
        {
            //        setError(e.what());
            logger_->error("Error while publishing: {}", e.what());
        }
        
        return false;
    }
    
    void fetch(bool mustBeFresh, bool versioned = false)
    {
        objectReadyPayload_.contentMetaInfo_.reset();
        objectReadyPayload_.object_.reset();
        objectReadyPayload_.objectNamespace_ = nullptr;
        
        shared_ptr<Impl> me = shared_from_this();
        switch (handlerType_)
        {
            case HandlerType::None:
                namespace_->objectNeeded(mustBeFresh);
                logger_->debug("Data packet requested: {}", namespace_->getName().toUri());
                break;
            case HandlerType::Segmented:
                SegmentedObjectHandler(namespace_.get(), [this,me](Namespace& objectNamespace)
                                       {
                                           objectReadyPayload_.objectNamespace_ = &objectNamespace;
                                           objectReadyPayload_.object_ =  objectNamespace.getObject();
                                       }).objectNeeded(mustBeFresh);
                logger_->debug("Segmented data requested {}", namespace_->getName().toUri());
                break;
            case HandlerType::GObj:
            {
                GeneralizedObjectHandler hndlr(namespace_.get(),
                                               [this,me]
                                               (const shared_ptr<ContentMetaInfoObject> &contentMetaInfo, Namespace &objectNamespace)
                                               {
                                                   objectReadyPayload_.objectNamespace_ = &objectNamespace;
                                                   objectReadyPayload_.object_ =  objectNamespace.getObject();
                                                   objectReadyPayload_.contentMetaInfo_ = contentMetaInfo;
                                               });
                hndlr.setNComponentsAfterObjectNamespace(versioned ? 1 : 0);
                hndlr.objectNeeded(mustBeFresh);
                logger_->debug("Generalized Object requested {}", namespace_->getName().toUri());
            }
                break;
            case HandlerType::GObjStream:
                break;
            default:
                break;
        }
    }
    
    static vector<pair<string,string>> datInfoFromObjectPayload(ObjectReadyPayload &p)
    {
        vector<pair<string,string>> rows;
        if (p.object_ && p.objectNamespace_)
        {
            vector<shared_ptr<Data>> allData;
            p.objectNamespace_->getAllData(allData);
            rows.push_back(pair<string,string>("Object Namespace", p.objectNamespace_->getName().toUri()));
            rows.push_back(pair<string,string>("Total Packets", to_string(allData.size())));
            if (p.contentMetaInfo_)
            {
                rows.push_back(pair<string,string>("Content-Type", p.contentMetaInfo_->getContentType()));
                rows.push_back(pair<string,string>("Timestamp", to_string(p.contentMetaInfo_->getTimestamp())));
                rows.push_back(pair<string,string>("Other", p.contentMetaInfo_->getOther().toRawStr()));
            }
            int idx = 0;
            for (auto d: allData)
                rows.push_back(pair<string,string>("Packet "+to_string(idx++), d->getName().toUri()));
        }
        return rows;
    }
};

NamespaceDAT::NamespaceDAT(const OP_NodeInfo* info)
: BaseDAT(info)
, freshness_(16000)
, faceDatOp_(nullptr)
, keyChainDatOp_(nullptr)
, rawOutput_(true)
, payloadInput_("")
, payloadOutput_("")
, mustBeFresh_(true)
, produceOnRequest_(false)
, gobjVersioned_(false)
, datInputData_(make_shared<DatInputData>())
, pimpl_(make_shared<NamespaceDAT::Impl>(logger_))
{
    *datInputData_ = { pimpl_->handlerType_, "", "text/html",
        make_shared<MetaInfo>(), shared_ptr<Blob>(), shared_ptr<Blob>()};
  
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
    
    if (!pimpl_->namespace_)
        initNamespace(output, inputs, reserved);
    
    if (pimpl_->namespace_ && pimpl_->namespace_->getFace_())
    {
        bool isFetching = !isProducer(inputs);
        switch (pimpl_->namespace_->getState()) {
            case NamespaceState_NAME_EXISTS:
            {
                if (isFetching)
                    runFetch(output, inputs, reserved);
                else
                {
                    copyDatInputData(output, inputs, reserved);
                    
                    if (!produceOnRequest_ &&
                        !(gobjVersioned_ && pimpl_->handlerType_ == HandlerType::GObj))
                        // active producer
                        runPublish(output, inputs, reserved);
                }
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
    if (pimpl_->getIsObjectReady())
    {
        if (!outputString_.size())
        {
            shared_ptr<BlobObject> b = dynamic_pointer_cast<BlobObject>(pimpl_->objectReadyPayload_.object_);
            if (b)
            {
                clearError();
                outputString_ = rawOutput_ ? b->toRawStr() : BaseDAT::toBase64(*b->getBlob());
            }
            else
            {
                setError("Failed to process received object");
                OPLOG_ERROR("Failed to cast received Object to BlobObject");
            }
        }
        
        output->setText(outputString_.c_str());
        
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
    
    if (pimpl_->getIsObjectReady() &&
        storePayload)
    {
        shared_ptr<BlobObject> b = dynamic_pointer_cast<BlobObject>(pimpl_->objectReadyPayload_.object_);
        if (!b)
        {
            OPLOG_ERROR("Failed to cast received Object to BlobObject");
            return;
        }
        
        if (retrieveOp(getCanonical(payloadOutput_)))
        {
            // save to TOP
            PayloadTOP *payloadTOP = (PayloadTOP*)retrieveOp(getCanonical(payloadOutput_));
            assert(payloadTOP);
            if (pimpl_->objectReadyPayload_.contentMetaInfo_)
            {
                string jsonErr;
                json11::Json json = json11::Json::parse(pimpl_->objectReadyPayload_.contentMetaInfo_->getOther().toRawStr(), jsonErr);
                if (jsonErr.size() == 0)
                {
                    int w = json["width"].int_value();
                    int h = json["height"].int_value();
                    
                    clearError();
                    payloadTOP->setBuffer(*b->getBlob(), w, h);
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
                file.write((const char*)b->getBlob().buf(), b->getBlob().size());
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

void
NamespaceDAT::copyDatInputData(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    datInputData_->inputFile_ = "";
    datInputData_->handlerType_ = pimpl_->handlerType_;
    datInputData_->metaInfo_->setFreshnessPeriod(freshness_);
    
    if (inputs->getNumInputs())
    {
        const OP_DATInput *datInput = inputs->getInputDAT(0);
        if (datInput->numCols == 0 || datInput->numRows == 0)
            // got nothing
            return ;
        
        datInputData_->contentType_ = (datInput->isTable && datInput->numRows > 1 ? datInput->getCell(1, 0) : "text/html");
        datInputData_->payload_ = make_shared<Blob>(Blob::fromRawStr(datInput->getCell(0, 0)));
        datInputData_->other_.reset();
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
            datInputData_->contentType_ = "image/x-bgra";
            
            json11::Json json = json11::Json::object {
                { "width", w },
                { "height", h },
                { "size", size }
            };
            datInputData_->other_ = make_shared<Blob>(Blob::fromRawStr(json.dump()));
            datInputData_->payload_ = make_shared<Blob>(buffer);
        }
        else
            datInputData_->inputFile_ = payloadInput_;
    }
}

bool
NamespaceDAT::isInputFile(const OP_Inputs *inputs) const
{
    return !(inputs->getNumInputs() || retrieveOp(getCanonical(payloadInput_)));
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
        payloadStored_ = false;
        pimpl_->initNamespace(prefix_, keyChain);
        
        if (isProducer)
        {
            if (produceOnRequest_ ||
                (gobjVersioned_ && pimpl_->handlerType_ == HandlerType::GObj))
            {
                bool versioned = (gobjVersioned_ && pimpl_->handlerType_ == HandlerType::GObj);
                shared_ptr<DatInputData> copiedPayload = datInputData_;
                pimpl_->produceOnRequest([copiedPayload]()
                {
                    return Impl::PayloadData::fromDatInputData(copiedPayload);
                }, versioned);
            }
            
            OPLOG_DEBUG("Created producer namespace {}", pimpl_->namespace_->getName().toUri());
        }
        else
            OPLOG_DEBUG("Created consumer namespace {}", pimpl_->namespace_->getName().toUri());

        pimpl_->setFace(faceDatOp_->getFaceProcessor(), isProducer);
    }
}

void
NamespaceDAT::releaseNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved)
{
    pimpl_->releaseNamespace();
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
    if ((!datInputData_->payload_ || datInputData_->payload_->size() == 0) && datInputData_->inputFile_.size() == 0)
    {
        setWarning("Can't read input. Not publishing");
        return;
    }
    
    clearWarning();
    clearError();
    pimpl_->produceNow(*pimpl_->namespace_, Impl::PayloadData::fromDatInputData(datInputData_),
                       gobjVersioned_ && pimpl_->handlerType_ == HandlerType::GObj);
}

void
NamespaceDAT::runFetch(DAT_Output *output, const OP_Inputs *inputs, void *reserved)
{
    pimpl_->fetch(mustBeFresh_, gobjVersioned_);
    outputString_ = "";
    payloadStored_ = false;
}

#define NDEFAULT_ROWS 3
bool
NamespaceDAT::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
    BaseDAT::getInfoDATSize(infoSize, reserved1);
    size_t packetsRow = pimpl_->getIsObjectReady() ? Impl::datInfoFromObjectPayload(pimpl_->objectReadyPayload_).size() : 0;
    int nDefaultRows = NDEFAULT_ROWS;
    
    infoSize->rows += nDefaultRows + (int)packetsRow;
    infoSize->cols = 2;
    infoSize->byColumn = false;
    
    return true;
}

void
NamespaceDAT::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries,
                                void* reserved1)
{
    vector<pair<string,string>> packetsRows = Impl::datInfoFromObjectPayload(pimpl_->objectReadyPayload_);
    int nDefaultRows = NDEFAULT_ROWS;
    int nRows = nDefaultRows + (int)packetsRows.size();;
    if (index < nRows)
    {
        switch (index) {
            case 0:
                entries->values[0]->setString("Full Name");
                entries->values[1]->setString( pimpl_->namespace_ ? pimpl_->namespace_->getName().toUri().c_str() : "" );
                break;
            case 1:
                entries->values[0]->setString("State");
                entries->values[1]->setString( pimpl_->namespace_ ? NamespaceStateMap.at(pimpl_->namespace_->getState()).c_str() : "n/a" );
                break;
            case 2:
                entries->values[0]->setString("Prefix Registered");
                entries->values[1]->setString(pimpl_->prefixRegistered_ ? "true" : "false");
                break;
            default:
                int i = index - nDefaultRows;
                entries->values[0]->setString(packetsRows[i].first.c_str());
                entries->values[1]->setString(packetsRows[i].second.c_str());
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
        if (pimpl_->namespace_ && pimpl_->namespace_->getFace_())
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
             if (it.second == pimpl_->handlerType_)
             {
                 p.defaultValue = it.first.c_str();
                 break;
             }
         return manager->appendMenu(p, PAR_HANDLER_MENU_SIZE, names, labels);
     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_GOBJ_VERSIONED, PAR_GOBJ_VERSIONED_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.defaultValues[0] = gobjVersioned_;
         return manager->appendToggle(p);
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
    
    appendPar<OP_NumericParameter>
    (manager, PAR_RAWOUTPUT, PAR_RAWOUTPUT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.defaultValues[0] = rawOutput_;
         return manager->appendToggle(p);
     });
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
    (PAR_HANDLER_TYPE, pimpl_->handlerType_, HandlerTypeMap.at(inputs->getParString(PAR_HANDLER_TYPE)));
    
    updateIfNew<bool>
    (PAR_GOBJ_VERSIONED, gobjVersioned_, (bool)inputs->getParInt(PAR_GOBJ_VERSIONED));
    
    updateIfNew<string>
    (PAR_INPUT, payloadInput_, inputs->getParString(PAR_INPUT));
    
    updateIfNew<string>
    (PAR_OUTPUT, payloadOutput_, inputs->getParString(PAR_OUTPUT));
    
    updateIfNew<bool>
    (PAR_RAWOUTPUT, rawOutput_, (bool)inputs->getParInt(PAR_RAWOUTPUT));
    
    // update parameters availability
    inputs->enablePar(PAR_GOBJ_VERSIONED, pimpl_->handlerType_ == HandlerType::GObj);
    bool isProducing = isProducer(inputs);
    inputs->enablePar(PAR_FRESHNESS, isProducing);
    inputs->enablePar(PAR_OUTPUT, !isProducing);
//    inputs->enablePar(PAR_INPUT, isProducing);
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
        outputString_ = "";
        if (pimpl_->getIsObjectReady())
            dispatchOnExecute(bind(&NamespaceDAT::setOutput, this, _1, _2, _3));
    });
    
    runIfUpdated(PAR_OUTPUT, [this](){
        if (pimpl_->getIsObjectReady())
            dispatchOnExecute(bind(&NamespaceDAT::storeOutput, this, _1, _2, _3));
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

shared_ptr<ndn::Blob>
NamespaceDAT::readFile(const std::string& fname, string& contentType, shared_ptr<Blob>& other)
{
    ifstream file(fname, ios_base::binary | ios_base::in | ios_base::ate);
    shared_ptr<Blob> b;
    if (file.is_open())
    {
        streamsize sz = file.tellg();
        shared_ptr<vector<uint8_t>> buf = make_shared<vector<uint8_t>>(sz);
        file.seekg(0);
        
        getModuleLogger()->trace("Reading file {} of size {}", fname, sz);
        if (!file.read((char*)(buf->data()), sz))
        {
            getModuleLogger()->error("Failed to read {} bytes from file {}", sz, fname);
        }
        else
        {
            b = make_shared<Blob>(buf);
            // TODO: set content type according to the extension
            contentType = "text/html";
        }
    }
    else
        getModuleLogger()->error("Failed to open file {}", fname);
    
    return b;
}


