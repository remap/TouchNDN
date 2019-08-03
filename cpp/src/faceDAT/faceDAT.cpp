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


#include "faceDAT.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <array>
#include <unordered_map>

#include <touchndn-helper/helper.hpp>

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/key-locator.hpp>
#include <ndn-cpp/security/key-chain.hpp>

#include "faceDAT-external.hpp"
#include "face-processor.hpp"
#include "keyChainDAT.h"
#include "key-chain-manager.hpp"

#define PAR_NFD_HOST "Nfdhost"
#define PAR_NFD_HOST_LABEL "NFD Host"
#define PAR_EXPRESS "Express"
#define PAR_EXPRESS_LABEL "Force Express"
#define PAR_LIFETIME "Lifetime"
#define PAR_LIFETIME_LABEL "Interest Lifetime"
#define PAR_MUSTBEFRESH "Mustbefresh"
#define PAR_MUSTBEFRESH_LABEL "MustBeFresh"

#define PAR_PAGE_OUTPUT "Output"
#define PAR_OUT_INTEREST "Interest"
#define PAR_OUT_STATUS "Status"
#define PAR_OUT_PAYLOAD_SIZE "Payloadsize"
#define PAR_OUT_DATA_SIZE "Datasize"
#define PAR_OUT_DATA_FRESHNESS "Datafreshness"
#define PAR_OUT_DATA_KEYLOCATOR "Keylocator"
#define PAR_OUT_DATA_SIGNATURE "Signature"
#define PAR_OUT_DATA_NAME "Dataname"
#define PAR_OUT_DRD "Drd"
#define PAR_OUT_FULLNAME "Fullname"
#define PAR_OUT_HEADERS "Headers"
#define PAR_OUT_RAWSTR "Rawstr"
#define PAR_OUTPUT_DATA "Data"

#define PAR_KEYCHAIN_DAT "Keychaindat"
#define PAR_KEYCHAIN_DAT_LABEL "KeyChain DAT"

#define INPUT_COLIDX_NAME 0
#define INPUT_COLIDX_LIFETIME 1
#define INPUT_COLIDX_FRESH 2

using namespace std;
using namespace std::placeholders;
using namespace ndn;
using namespace touch_ndn;

//******************************************************************************
// InfoDAT and InfoCHOP labels and indexes
const map<FaceDAT::InfoChopIndex, string> FaceDAT::ChanNames = {
    { FaceDAT::InfoChopIndex::FaceProcessing, "faceProcessing" },
    { FaceDAT::InfoChopIndex::RequestsTableSize, "requestsTableSize" },
    { FaceDAT::InfoChopIndex::ExpressedNum, "expressedNum" }
};

enum class Outputs : int32_t {
    Interest,
    Status,
    DataName,
    PayloadSize,
    DataSize,
    Freshness,
    Keylocator,
    Signature,
    Drd
};


const unordered_map<string, string> OutputLabels = {
    { PAR_OUT_HEADERS, "Header" },
    { PAR_OUT_FULLNAME, "Full Data Name" },
    { PAR_OUT_RAWSTR, "Raw" },
    { PAR_OUT_INTEREST, "Interest" },
    { PAR_OUT_STATUS, "Status" },
    { PAR_OUT_DATA_NAME, "Data Name" },
    { PAR_OUT_PAYLOAD_SIZE, "Payload Size" },
    { PAR_OUT_DATA_SIZE, "Data Size" },
    { PAR_OUT_DATA_FRESHNESS, "Data Freshness" },
    { PAR_OUT_DATA_KEYLOCATOR, "Keylocator" },
    { PAR_OUT_DATA_SIGNATURE, "Signature" },
    { PAR_OUT_DRD, "Data Retrieval Delay" }
};

// this table defines column ordering in the output table
const map<Outputs, string> OutputsMap = {
    { Outputs::Interest, PAR_OUT_INTEREST },
    { Outputs::Status, PAR_OUT_STATUS },
    { Outputs::DataName, PAR_OUT_DATA_NAME },
    { Outputs::PayloadSize, PAR_OUT_PAYLOAD_SIZE },
    { Outputs::DataSize, PAR_OUT_DATA_SIZE },
    { Outputs::Freshness, PAR_OUT_DATA_FRESHNESS },
    { Outputs::Keylocator, PAR_OUT_DATA_KEYLOCATOR },
    { Outputs::Signature, PAR_OUT_DATA_SIGNATURE },
    { Outputs::Drd, PAR_OUT_DRD }
};

//******************************************************************************
FaceDAT::FaceDAT(const OP_NodeInfo* info)
: BaseDAT(info)
, nfdHost_("localhost")
, mustBeFresh_(false)
, lifetime_(4000)
, requestsTable_(make_shared<RequestsTable>())
, showHeaders_(true)
, showFullName_(false)
, showRawStr_(false)
, forceExpress_(false)
, instanceCertRegId_(0)
, signingCertRegId_(0)
, keyChainDat_("")
, keyChainDatOp_(nullptr)
{
    currentOutputs_ = {PAR_OUT_HEADERS, PAR_OUT_DRD, PAR_OUT_INTEREST, PAR_OUT_DATA_NAME, PAR_OUT_PAYLOAD_SIZE, PAR_OUT_STATUS, PAR_OUT_RAWSTR};
    dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
    OPLOG_DEBUG("Created FaceDAT");
}

FaceDAT::~FaceDAT()
{
    cancelRequests();
    clearKeyChainPairing(nullptr, nullptr, nullptr);
    doCleanup();
    OPLOG_DEBUG("Released FaceDAT");
}

void
FaceDAT::execute(DAT_Output* output, const OP_Inputs* inputs, void* reserved)
{
    BaseDAT::execute(output, inputs, reserved);

    if (inputs->getNumInputs() > 0 && faceProcessor_)
    {
        vector<shared_ptr<Interest>> inputInterests;
        const OP_DATInput    *cinput = inputs->getInputDAT(0);
        bool isTable = cinput->isTable;

        if (!isTable) // is Text
        {
            // split by newline
            stringstream ss(cinput->getCell(0, 0));
            string prefix;
            while (getline(ss, prefix))
            {
                if (prefix.size())
                {
                    shared_ptr<Interest> i = make_shared<Interest>(prefix);
                    i->setInterestLifetimeMilliseconds(lifetime_);
                    i->setMustBeFresh(mustBeFresh_);
                    inputInterests.push_back(i);
                }
            }
        }
        else
        {
            int numRows = cinput->numRows;
            int numCols = cinput->numCols;
            for (int rowIdx = 0; rowIdx < numRows; ++rowIdx)
            {
                string prefix(cinput->getCell(rowIdx, INPUT_COLIDX_NAME));
                int lifetime = lifetime_;
                bool mustBeFresh = mustBeFresh_;
                
                if (INPUT_COLIDX_LIFETIME < numCols)
                {
                    string s = cinput->getCell(rowIdx, INPUT_COLIDX_LIFETIME);
                    try {
                        if (s.size()) lifetime = stoi(s);
                    }
                    catch (std::exception &e)
                    {
                        OPLOG_ERROR("Exception {0}", e.what());
                    }
                }
                
                if (INPUT_COLIDX_FRESH < numCols)
                {
                    string s = cinput->getCell(rowIdx, INPUT_COLIDX_FRESH);
                    transform(s.begin(), s.end(), s.begin(), ::tolower);
                    if (s.size()) mustBeFresh = (s == "true" ? true : false);
                }
                
                if (prefix.size())
                {
                    shared_ptr<Interest> i = make_shared<Interest>(prefix);
                    i->setInterestLifetimeMilliseconds(lifetime);
                    i->setMustBeFresh(mustBeFresh);
                    inputInterests.push_back(i);
                }
            }
        }
        
        // check if we need to express new interests
        // cancel all requests and flush table if input has new data or any interest parameter has changed
        bool flushTable = false;
        requestsTable_->acquire([&inputInterests, &flushTable](RequestsDict &d){
            for (auto i : inputInterests)
            {
                RequestsDict::iterator it = d.find(i->getName().toUri());
                if (it == d.end())
                    flushTable = true;
                else
                    flushTable = (i->getInterestLifetimeMilliseconds() != it->second.interest_->getInterestLifetimeMilliseconds() ||
                                  i->getMustBeFresh() != it->second.interest_->getMustBeFresh());
                if (flushTable)
                    break;
            }
        });
        
        if (flushTable || forceExpress_)
        {
            cancelRequests();
            express(inputInterests, flushTable);
        }
    }
    
    outputRequestsTable(output);
    forceExpress_ = false;
}

int32_t
FaceDAT::getNumInfoCHOPChans(void* reserved1)
{
    return BaseDAT::getNumInfoCHOPChans(reserved1) + (int32_t) ChanNames.size();
}

void
FaceDAT::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
{
    FaceDAT::InfoChopIndex idx = (FaceDAT::InfoChopIndex)index;
    
    if (index < ChanNames.size())
    {
        chan->name->setString(ChanNames.at(idx).c_str());

        switch (idx) {
            case FaceDAT::InfoChopIndex::FaceProcessing:
            {
                chan->value = faceProcessor_ && faceProcessor_->isProcessing();
            }
                break;
            case FaceDAT::InfoChopIndex::RequestsTableSize:
            {
                chan->value = requestsTable_->dict_.size();
            }
                break;
            case FaceDAT::InfoChopIndex::ExpressedNum:
            {
                chan->value = nExpressed_;
            }
                break;
            default:
            {
                chan->value = 0;
                stringstream ss;
                ss << "n_a_" << index;
                chan->name->setString(ss.str().c_str());
            }
                break;
        }
    }
    else
        BaseDAT::getInfoCHOPChan(index - (int32_t)ChanNames.size(), chan, reserved1);
}

bool
FaceDAT::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
    BaseDAT::getInfoDATSize(infoSize, reserved1);
    
    infoSize->rows += (keyChainDatOp_ ? 1 : 0);
	infoSize->rows += registeredPrefixes_.size();
    
	infoSize->cols = 2;
	infoSize->byColumn = false;
	return true;
}

void
FaceDAT::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries,
                           void* reserved1)
{
    size_t nRows = registeredPrefixes_.size() + (keyChainDatOp_ ? 1 : 0);
    
    if (index < nRows)
    {
        if (keyChainDatOp_ && index == 0)
        {
            entries->values[0]->setString("KeyChainDAT");
            entries->values[1]->setString(keyChainDat_.c_str());
        }
        else
        {
            if (keyChainDatOp_) index -= 1;
            
            map<uint64_t, string>::iterator it = registeredPrefixes_.begin();
            advance(it, index);
            stringstream ss;
            ss << "Registered " << index;
            entries->values[0]->setString(ss.str().c_str());
            entries->values[1]->setString(it->second.c_str());
        }
    }
    else
        BaseDAT::getInfoDATEntries((int32_t)(index - nRows), nEntries, entries, reserved1);
}

void
FaceDAT::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
    BaseDAT::setupParameters(manager, reserved1);
    
    appendPar<OP_StringParameter>
    (manager, PAR_NFD_HOST, PAR_NFD_HOST_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         p.defaultValue = nfdHost_.c_str();
         return manager->appendString(p);
     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_EXPRESS, PAR_EXPRESS_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         return manager->appendPulse(p);
     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_LIFETIME, PAR_LIFETIME_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.defaultValues[0] = lifetime_;
         p.minValues[0] = 0;
         p.maxValues[0] = 24*3600*1000;
         p.minSliders[0] = 0;
         p.maxSliders[0] = p.maxValues[0];
         
         return manager->appendInt(p);
     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_MUSTBEFRESH, PAR_LIFETIME_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.defaultValues[0] = mustBeFresh_;
         
         return manager->appendToggle(p);
     });
    
    appendPar<OP_StringParameter>
    (manager, PAR_KEYCHAIN_DAT, PAR_KEYCHAIN_DAT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
    });
    
    // outputs page
    for (auto p : OutputLabels)
        
        appendPar<OP_NumericParameter>
        (manager, p.first, OutputLabels.at(p.first), PAR_PAGE_OUTPUT,
         [&](OP_NumericParameter &p){
             bool enabled = (currentOutputs_.find(p.name) != currentOutputs_.end() ? true : false);
             p.defaultValues[0] = enabled;
             return manager->appendToggle(p);
         });
}

void
FaceDAT::pulsePressed(const char *name, void *reserved1)
{
    BaseDAT::pulsePressed(name, reserved1);
    
    if (strcmp(name, PAR_EXPRESS) == 0)
    {
        // do nothing? since pulse will force cook...
        forceExpress_ = true;
    }
}

void
FaceDAT::initPulsed()
{
    dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
}

void
FaceDAT::initFace(DAT_Output*output, const OP_Inputs* inputs, void* reserved)
{
    if (keyChainDatOp_)
        clearKeyChainPairing(output, inputs, reserved);
    
    faceProcessor_.reset();
    
    try
    {
        std::string hostname(inputs->getParString(PAR_NFD_HOST));
        if (helpers::FaceProcessor::checkNfdConnection(hostname))
        {
            clearError();
            faceProcessor_ = make_shared<helpers::FaceProcessor>(hostname);
            faceProcessor_->start();
            setIsReady(true);
        }
        else
            setError("Can't connect to NFD");
    }
    catch (std::runtime_error &e)
    {
        setError("Can't connect to NFD");
        OPLOG_ERROR("Can't connect to NFD");
    }
}

void
FaceDAT::checkParams(DAT_Output *, const OP_Inputs *inputs,
                     void *reserved)
{
    updateIfNew<string>
    (PAR_NFD_HOST, nfdHost_, inputs->getParString(PAR_NFD_HOST));
    
    if (faceProcessor_)
    {
        updateIfNew<string>
        (PAR_KEYCHAIN_DAT, keyChainDat_, getCanonical(inputs->getParString(PAR_KEYCHAIN_DAT)));
    }
    
    currentOutputs_.clear();
    for (auto p : OutputsMap)
    {
        int val = inputs->getParInt(p.second.c_str());
        if (val == 1) currentOutputs_.insert(p.second);
    }
    
    showHeaders_ = inputs->getParInt(PAR_OUT_HEADERS);
    showFullName_ = inputs->getParInt(PAR_OUT_FULLNAME);
    showRawStr_ = inputs->getParInt(PAR_OUT_RAWSTR);
    lifetime_ = inputs->getParInt(PAR_LIFETIME);
    mustBeFresh_ = inputs->getParInt(PAR_MUSTBEFRESH);
}

void
FaceDAT::paramsUpdated()
{
    runIfUpdated(PAR_NFD_HOST, [this](){
        dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
    });
    runIfUpdated(PAR_KEYCHAIN_DAT, [this](){
        // clear up existing keychain, if set up
        if (keyChainDatOp_)
            dispatchOnExecute(bind(&FaceDAT::clearKeyChainPairing, this, _1, _2, _3));
        dispatchOnExecute(bind(&FaceDAT::setupKeyChainPairing, this, _1, _2, _3));
    });
}

void
FaceDAT::express(const vector<shared_ptr<Interest>>& interests, bool clearTable)
{
    for (auto i : interests)
    {
        express(i, clearTable);
        clearTable = false;
    }
}

void
FaceDAT::express(std::string prefix, int lifetime, bool mustBeFresh, bool clearTable)
{
    shared_ptr<Interest> i = make_shared<Interest>(prefix);
    i->setInterestLifetimeMilliseconds(lifetime);
    i->setMustBeFresh(mustBeFresh);
    express(i);
}

void
FaceDAT::express(shared_ptr<Interest> &i, bool clearTable)
{
    shared_ptr<RequestsTable> rt = requestsTable_;
    shared_ptr<helpers::logger> logger = logger_;
    nExpressed_++;
    faceProcessor_->dispatchSynchronized([i, rt, clearTable, logger](shared_ptr<Face> f){
        if (clearTable)
            rt->acquire([](RequestsDict &d){
                d.clear();
            });

        // NOTE: callbacks are called on Face thread!
        rt->cancelIfPending(*i, *f);
        uint64_t piId = f->expressInterest(*i,
                           [rt,logger](const shared_ptr<const Interest>& i, const shared_ptr<Data>& d){
                               rt->setData(i, d);
                               logger->trace("Received data {0}", d->getName().toUri());
                           },
                           [rt,logger](const shared_ptr<const Interest>& i){
                               rt->setTimeout(i);
                               logger->trace("Timeout {}", i->getName().toUri());
                           },
                           [rt,logger](const shared_ptr<const Interest>& i, const shared_ptr<NetworkNack>& nack){
                               rt->setNack(i, nack);
                               logger->trace("Nack {}", i->getName().toUri());
                           });
        assert(rt->setExpressed(i, piId));
    });
}

void
FaceDAT::cancelRequests()
{
    // cancel all pending requests and quit
    shared_ptr<helpers::logger> logger = logger_;
    shared_ptr<RequestsTable> rt = requestsTable_;
    if (faceProcessor_) faceProcessor_->dispatchSynchronized([rt,logger](shared_ptr<Face> f){
            rt->acquire([&](RequestsDict &d){
                for (auto it : d)
                {
                    f->removePendingInterest(it.second.pitId_);
                    it.second.isCanceled_ = true;
                    logger->trace("Canceled pending {0}", it.first);
                }
            });
        });
}

void
FaceDAT::outputRequestsTable(DAT_Output *output)
{
    if (requestsTable_->dict_.size())
    {
        output->setTableSize((int32_t)requestsTable_->dict_.size()+showHeaders_, (int32_t)currentOutputs_.size()+1);
        
        // set headers
        int colIdx = 0;
        int rowIdx = 0;
        if (showHeaders_)
        {
            for (auto p : OutputsMap)
                if (currentOutputs_.find(p.second) != currentOutputs_.end())
                {
                    string header = OutputLabels.at(p.second);
                    output->setCellString(0, colIdx, header.c_str());
                    colIdx++;
                }
            output->setCellString(0, colIdx, PAR_OUTPUT_DATA);
            rowIdx++;
        }
        
        requestsTable_->acquire([output, &rowIdx, this](RequestsDict &d){
            for (auto p : d)
                setOutputEntry(output, p, rowIdx++);
        });
    }
    else
    {
        output->setOutputDataType(DAT_OutDataType::Text);
        output->setText("");
    }
}

void FaceDAT::setOutputEntry(DAT_Output *output, RequestsDictPair &p, int row)
{
    int colIdx = 0;
    for (auto l : OutputsMap)
    {
        if (currentOutputs_.find(l.second) != currentOutputs_.end())
        {
            switch (l.first) {
                case Outputs::Interest:
                    output->setCellString(row, colIdx, p.first.c_str());
                    break;
                case Outputs::DataName:
                    if (p.second.data_)
                    {
                        Name n = (showFullName_ ? *(p.second.data_->getFullName()) : p.second.data_->getName());
                        
                        output->setCellString(row, colIdx, n.toUri().c_str());
                    }
                    else
                        output->setCellString(row, colIdx, "");
                    break;
                case Outputs::Status:
                {
                    string status = p.second.isCanceled_ ? "canceled" : "pending";
                    if (p.second.isDone())
                        status = (p.second.data_ ? "data" : (p.second.isTimeout_ ? "timeout" : "nack"));
                    output->setCellString(row, colIdx, status.c_str());
                }
                    break;
                case Outputs::PayloadSize:
                    output->setCellInt(row, colIdx,
                                       p.second.data_ ? (int32_t)p.second.data_->getContent().size() : 0);
                    break;
                case Outputs::DataSize:
                    output->setCellInt(row, colIdx,
                                       p.second.data_ ? (int32_t)p.second.data_->getDefaultWireEncoding().size() : 0);
                    break;
                case Outputs::Freshness:
                    output->setCellInt(row, colIdx,
                                       p.second.data_ ? (int32_t)p.second.data_->getMetaInfo().getFreshnessPeriod() : 0);
                    break;
                case Outputs::Keylocator:
                    if (p.second.data_)
                    {
                        Name n = KeyLocator::getFromSignature(p.second.data_->getSignature()).getKeyName();
                        output->setCellString(row, colIdx, n.toUri().c_str());
                    }
                    else
                        output->setCellString(row, colIdx, "");
                    break;
                case Outputs::Signature:
                    if (p.second.data_)
                        output->setCellString(row, colIdx,
                                              BaseDAT::toBase64(p.second.data_->getSignature()->getSignature()).c_str());
                    else
                        output->setCellString(row, colIdx, "");
                    break;
                case Outputs::Drd:
                    output->setCellInt(row, colIdx, p.second.getDrd());
                    break;
                default:
                    break;
            }
            colIdx++;
        }
    }
    
    if (p.second.data_)
    {
        if (!showRawStr_)
            output->setCellString(row, colIdx,
                                  BaseDAT::toBase64(p.second.data_->getContent()).c_str());
        else
            output->setCellString(row, colIdx,
                                  p.second.data_->getContent().toRawStr().c_str());
    }
    else
        output->setCellString(row, colIdx, "");
}

void FaceDAT::setupKeyChainPairing(DAT_Output* output, const OP_Inputs* inputs, void* reserved)
{
    int err = 1;
    void *keyChainDatOp;
    
    if (keyChainDat_ == "")
    {
        err = 0;
        clearKeyChainPairing(output, inputs, reserved);
        keyChainDatOp_ = nullptr;
        clearError();
        setIsReady(true);
    }
    else if ((keyChainDatOp = retrieveOp(keyChainDat_)))
        {
            keyChainDatOp_ = (KeyChainDAT*)keyChainDatOp;
        
            err = (keyChainDatOp_ ? 0 : 1);
            
            if (keyChainDatOp_ && keyChainDatOp_->getKeyChainManager())
            {
                clearError();
                setIsReady(true);
                // watch keychain operator events
                keyChainDatOp_->subscribe(this);
                
                shared_ptr<helpers::KeyChainManager> kcm = keyChainDatOp_->getKeyChainManager();
                // set command signging info
                // TODO: is it safe to capture this here?
                faceProcessor_->dispatchSynchronized([kcm, this](shared_ptr<Face> face){
                    face->setCommandSigningInfo(*kcm->instanceKeyChain(),
                                                kcm->instanceKeyChain()->getDefaultCertificateName());
                    kcm->instanceKeyChain()->setFace(face.get());
                    registerCertPrefixes(face, kcm);
                });
            }
        }
    
    if (err)
        setError("Couldn't find KeyChainDAT: %s", keyChainDat_.c_str());
}

void FaceDAT::clearKeyChainPairing(DAT_Output *, const OP_Inputs *, void *reserved)
{
    if (keyChainDatOp_)
    {
        OPLOG_DEBUG("Clear KeyChainDAT");
        
        shared_ptr<helpers::KeyChainManager> kcm = keyChainDatOp_->getKeyChainManager();
        uint64_t signingCertRegId = signingCertRegId_;
        uint64_t instanceCertRegId = instanceCertRegId_;
        faceProcessor_->dispatchSynchronized([signingCertRegId, instanceCertRegId, kcm](shared_ptr<Face> face){
            face->removeRegisteredPrefix(signingCertRegId);
            if (instanceCertRegId != signingCertRegId)
                face->removeRegisteredPrefix(instanceCertRegId);

            // clear KeyChain's Face
            kcm->instanceKeyChain()->setFace(nullptr);
            // clear Face's keychain
            face->setCommandSigningInfo(*(KeyChain*)0, Name());
        });
        
        registeredPrefixes_.erase(signingCertRegId_);
        registeredPrefixes_.erase(instanceCertRegId_);
        signingCertRegId_ = 0;
        instanceCertRegId_ = 0;
        keyChainDat_ = "";
        keyChainDatOp_->unsubscribe(this);
        keyChainDatOp_ = nullptr;
    }
}

void FaceDAT::registerCertPrefixes(std::shared_ptr<ndn::Face> face, std::shared_ptr<helpers::KeyChainManager> kcm)
{
    shared_ptr<Data> signingCert = kcm->signingIdentityCertificate();
    shared_ptr<Data> instanceCert = kcm->instanceCertificate();
    OnInterestCallback onCertInterest = [signingCert, instanceCert]
            (const shared_ptr<const Name>&,
             const shared_ptr<const Interest> &i,
             Face& f, uint64_t,
             const shared_ptr<const InterestFilter>&)
            {
                if (i->getName().match(signingCert->getName()))
                    f.putData(*signingCert);
                if (i->getName().match(instanceCert->getName()))
                    f.putData(*instanceCert);
            };
    OnRegisterFailed onRegisterFailed = [this](const shared_ptr<const Name>& n){
        this->setError("Failed to register prefix: %s", n->toUri().c_str());
        OPLOG_ERROR("Failed to register prefix: {}", n->toUri());
    };
    OnRegisterSuccess onRegisterSuccess = [this](const shared_ptr<const Name>&n, uint64_t id){
        this->registeredPrefixes_[id] = n->toUri();
        OPLOG_INFO("Registered prefix {0}", n->toUri());
    };
    
    signingCertRegId_ = face->registerPrefix(signingCert->getName(), onCertInterest, onRegisterFailed, onRegisterSuccess);
    
    if (!signingCert->getName().isPrefixOf(instanceCert->getName()))
        instanceCertRegId_ = face->registerPrefix(instanceCert->getName(), onCertInterest, onRegisterFailed, onRegisterSuccess);
    else
        instanceCertRegId_ = signingCertRegId_;
}

void FaceDAT::doCleanup()
{
    // remove all registered prefixes
    for (auto it:registeredPrefixes_)
        faceProcessor_->dispatchSynchronized([it](shared_ptr<Face> face){
            face->removeRegisteredPrefix(it.first);
        });
    // unregister from KeyChainDAT
    if (keyChainDatOp_)
        keyChainDatOp_->unsubscribe(this);
    
}

void FaceDAT::onOpUpdate(OP_Common *op, const std::string& event)
{
    if (op == keyChainDatOp_)
    {
        if (event == OP_EVENT_DESTROY)
            clearKeyChainPairing(nullptr, nullptr, nullptr);
    }
}

//******************************************************************************
void FaceDAT::RequestsTable::acquireIfExists(const Interest &i, std::function<void (RequestsDict &, RequestsDict::iterator &it)> f)
{
    lock_guard<recursive_mutex> scopedLock(mx_);
    RequestsDict::iterator it = dict_.find(i.getName().toUri());
    if (it != dict_.end()) f(dict_, it);
}

bool FaceDAT::RequestsTable::cancelIfPending(const ndn::Interest &i, ndn::Face &f)
{
    bool res = false;
    
    acquireIfExists(i,[&](RequestsDict &d, RequestsDict::iterator &it){
        f.removePendingInterest(it->second.pitId_);
        d.erase(it);
        res = true;
    });
    return res;
}

bool FaceDAT::RequestsTable::setExpressed(const std::shared_ptr<const ndn::Interest>& i, uint64_t id)
{
    bool res = false;
    
    acquire([&](RequestsDict &d){
        // there should be no pending interests. use cancelIfPending to clear beforehand
        if (d.find(i->getName().toUri()) == d.end())
        {
            RequestStatus rs;
            rs.interest_ = i;
            rs.pitId_ = id;
            rs.isTimeout_ = false;
            rs.expressTs_ = rs.replyTs_ = ndn_getNowMilliseconds();

            d[i->getName().toUri()] = rs;
            res = true;
        }
    });
    return res;
}

bool FaceDAT::RequestsTable::setData(const std::shared_ptr<const ndn::Interest> &i, const std::shared_ptr<ndn::Data> &data)
{
    bool res = false;
    acquireIfExists(*i, [&](RequestsDict &d, RequestsDict::iterator &it){
        it->second.data_ = data;
        it->second.replyTs_ = ndn_getNowMilliseconds();
    });
    return res;
}

bool FaceDAT::RequestsTable::setTimeout(const std::shared_ptr<const ndn::Interest> &i)
{
    bool res = false;
    acquireIfExists(*i, [&](RequestsDict &d, RequestsDict::iterator &it){
        it->second.isTimeout_ = true;
    });
    return res;
}

bool FaceDAT::RequestsTable::setNack(const std::shared_ptr<const ndn::Interest> &i, const std::shared_ptr<ndn::NetworkNack> &n)
{
    bool res = false;
    acquireIfExists(*i, [&](RequestsDict &d, RequestsDict::iterator &it){
        it->second.nack_ = n;
    });
    return res;
}
