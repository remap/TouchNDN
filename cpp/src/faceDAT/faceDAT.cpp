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

#include <ndn-cpp/face.hpp>
#include <ndn-cpp/key-locator.hpp>

#include "face-processor.hpp"

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

#define INPUT_COLIDX_NAME 0
#define INPUT_COLIDX_LIFETIME 1
#define INPUT_COLIDX_FRESH 2

using namespace std;
using namespace std::placeholders;
using namespace ndn;
using namespace touch_ndn;

extern "C"
{

DLLEXPORT
void
FillDATPluginInfo(DAT_PluginInfo *info)
{
	info->apiVersion = DATCPlusPlusAPIVersion;
	info->customOPInfo.opType->setString("Touchndnface");
	info->customOPInfo.opLabel->setString("Face DAT");
	info->customOPInfo.opIcon->setString("FDT");
	info->customOPInfo.authorName->setString("Peter Gusev");
	info->customOPInfo.authorEmail->setString("peter@remap.ucla.edu");
	info->customOPInfo.minInputs = 0;
	info->customOPInfo.maxInputs = 1;
}

DLLEXPORT
DAT_CPlusPlusBase*
CreateDATInstance(const OP_NodeInfo* info)
{
	return new FaceDAT(info);
}

DLLEXPORT
void
DestroyDATInstance(DAT_CPlusPlusBase* instance)
{
	delete (FaceDAT*)instance;
}

};

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
{
//    for (auto p : OutputLabels)
//        currentOutputs_.insert(p.first);
    currentOutputs_ = {PAR_OUT_HEADERS, PAR_OUT_DRD, PAR_OUT_INTEREST, PAR_OUT_DATA_NAME, PAR_OUT_PAYLOAD_SIZE, PAR_OUT_STATUS, PAR_OUT_RAWSTR};
    dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
}

FaceDAT::~FaceDAT()
{
    cancelRequests();
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
                    if (s.size()) lifetime = stoi(s);
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
    
	infoSize->rows += 0;
	infoSize->cols += 0;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
FaceDAT::getInfoDATEntries(int32_t index,
									int32_t nEntries,
									OP_InfoDATEntries* entries,
									void* reserved1)
{
    BaseDAT::getInfoDATEntries(index, nEntries, entries, reserved1);
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
FaceDAT::initFace(DAT_Output*, const OP_Inputs* inputs, void* reserved)
{
    faceProcessor_.reset();
    
    try
    {
        std::string hostname(inputs->getParString(PAR_NFD_HOST));
        if (helpers::FaceProcessor::checkNfdConnection(hostname))
        {
            clearError();
            faceProcessor_ = make_shared<helpers::FaceProcessor>(hostname);
            faceProcessor_->start();
        }
        else
            setError("Can't connect to NFD");
    }
    catch (std::runtime_error &e)
    {
        setError("Can't connect to NFD");
    }
}

void
FaceDAT::checkInputs(set<string>& paramNames, DAT_Output *, const OP_Inputs *inputs,
                     void *reserved)
{
    
    if (nfdHost_ != string(inputs->getParString(PAR_NFD_HOST)))
    {
        paramNames.insert(PAR_NFD_HOST);
        nfdHost_ = string(inputs->getParString(PAR_NFD_HOST));
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
FaceDAT::paramsUpdated(const std::set<std::string> &updatedParams)
{
    if (updatedParams.find(PAR_NFD_HOST) != updatedParams.end())
    {
        dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
    }
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
    nExpressed_++;
    faceProcessor_->dispatchSynchronized([i, rt, clearTable](shared_ptr<Face> f){
        if (clearTable)
            rt->acquire([](RequestsDict &d){
                d.clear();
            });

        // NOTE: callbacks are called on Face thread!
        rt->cancelIfPending(*i, *f);
        uint64_t piId = f->expressInterest(*i,
                           [rt](const shared_ptr<const Interest>& i, const shared_ptr<Data>& d){
                               cout << "data    " << i->getName() << endl;
                               rt->setData(i, d);
                           },
                           [rt](const shared_ptr<const Interest>& i){
                               cout << "timeout " << i->getName() << endl;
                               rt->setTimeout(i);
                           },
                           [rt](const shared_ptr<const Interest>& i, const shared_ptr<NetworkNack>& nack){
                               cout << "nack    " << i->getName() << endl;
                               rt->setNack(i, nack);
                           });
        assert(rt->setExpressed(i, piId));
    });
}

void
FaceDAT::cancelRequests()
{
    // cancel all pending requests and quit
    shared_ptr<RequestsTable> rt = requestsTable_;
    if (faceProcessor_) faceProcessor_->dispatchSynchronized([rt](shared_ptr<Face> f){
            rt->acquire([&](RequestsDict &d){
                for (auto it : d)
                {
                    f->removePendingInterest(it.second.pitId_);
                    it.second.isCanceled_ = true;
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
