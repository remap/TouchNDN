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

#include "ndnrtcOut.hpp"

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <ndnrtc/stream.hpp>
#include <ndnrtc/name-components.hpp>
#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>
#include <libyuv.h>

#include "faceDat.h"
#include "keyChainDAT.h"
#include "face-processor.hpp"
#include "key-chain-manager.hpp"

#define MODULE_LOGGER "ndnrtcTOP"

#define GetError( )\
{\
for ( GLenum Error = glGetError( ); ( GL_NO_ERROR != Error ); Error = glGetError( ) )\
{\
switch ( Error )\
{\
case GL_INVALID_ENUM:      printf( "\n%s\n\n", "GL_INVALID_ENUM"      ); assert( 1 ); break;\
case GL_INVALID_VALUE:     printf( "\n%s\n\n", "GL_INVALID_VALUE"     ); assert( 1 ); break;\
case GL_INVALID_OPERATION: printf( "\n%s\n\n", "GL_INVALID_OPERATION" ); assert( 1 ); break;\
case GL_OUT_OF_MEMORY:     printf( "\n%s\n\n", "GL_OUT_OF_MEMORY"     ); assert( 1 ); break;\
default:                                                                              break;\
}\
}\
}

#define BASE_PREFIX "/touchdesigner"

#define PAR_FACEOP "Faceop"
#define PAR_FACEOP_LABEL "Face"
#define PAR_KEYCHAINOP "Keychain"
#define PAR_KEYCHAINOP_LABEL "KeyChain"
#define PAR_BITRATE "Bitrate"
#define PAR_BITRATE_LABEL "Bitrate"
#define PAR_USEFEC "Fec"
#define PAR_USEFEC_LABEL "FEC"
#define PAR_DROPFRAMES "Dropframes"
#define PAR_DROPFRAMES_LABEL "Allow Frame Drop"
#define PAR_SEGSIZE "Segsize"
#define PAR_SEGSIZE_LABEL "Segment Size"
#define PAR_GOP_SIZE "Gopsize"
#define PAR_GOP_SIZE_LABEL "GOP Size"

using namespace std;
using namespace std::placeholders;
using namespace touch_ndn;
using namespace ndn;
using namespace ndnrtc;

static string BasePrefix = getenv("TOUCHNDN_BASE_PREFIX") ? getenv("TOUCHNDN_BASE_PREFIX") : BASE_PREFIX ;

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
    FillTOPPluginInfo(TOP_PluginInfo *info)
    {
        info->apiVersion = TOPCPlusPlusAPIVersion;
        info->executeMode = TOP_ExecuteMode::CPUMemWriteOnly;
        info->customOPInfo.opType->setString("Touchndnrtcout");
        info->customOPInfo.opLabel->setString("NdnRtcOut TOP");
        info->customOPInfo.opIcon->setString("NOT");
        info->customOPInfo.authorName->setString("Peter Gusev");
        info->customOPInfo.authorEmail->setString("peter@remap.ucla.edu");
        info->customOPInfo.minInputs = 0;
        info->customOPInfo.maxInputs = 1;
    }
    
    DLLEXPORT
    TOP_CPlusPlusBase*
    CreateTOPInstance(const OP_NodeInfo* info)
    {
        return new NdnRtcOut(info);
    }
    
    DLLEXPORT
    void
    DestroyTOPInstance(TOP_CPlusPlusBase* instance)
    {
        delete (NdnRtcOut*)instance;
    }
};

class NdnRtcOut::Impl : public enable_shared_from_this<NdnRtcOut::Impl>
{
public:
    Impl(shared_ptr<spdlog::logger> l)
    : logger_(l)
    , prefixRegistered_(false)
    {}
    
    ~Impl(){
        releaseStream();
        cleanupFaceProcessor();
    }
    
    bool getIsInitialized() const { return stream_.get() != nullptr; }
    string getErrorString() const { return errorString_; }
    string getStreamPrefix() const { return stream_ ? stream_->getPrefix() : "n/a"; }
    uint32_t getFrameNumber() const { return stream_ ? lastFrame_.sampleNo_ : 0; }
    string getLastFramePrefix() const { return stream_ ? lastFrame_.getPrefix(NameFilter::Sample).toUri() : "n/a"; }
    statistics::StatisticsStorage getStats() const { return stream_->getStatistics(); }
    const NamespaceInfo& getLastFrameInfo() const { return lastFrame_; }
    
    void initStream(const string& base, const string &name,
                    const VideoStream::Settings& settings,
                    const int32_t& cacheLen,
                    shared_ptr<helpers::FaceProcessor> faceProcessor,
                    shared_ptr<KeyChain> keyChain)
    {
        errorString_ = "";
        setFaceProcessor(faceProcessor);
        
        shared_ptr<Impl> me = shared_from_this();
        faceProcessor_->dispatchSynchronized([this, me, base, name, settings, cacheLen, keyChain](shared_ptr<Face> f)
        {
            VideoStream::Settings s(settings);
            s.memCache_ = make_shared<MemoryContentCache>(f.get());
            s.memCache_->setMinimumCacheLifetime(cacheLen);
            stream_ = make_shared<VideoStream>(base, name, s, keyChain);
            logger_->info("Initialized NDN-RTC stream {}", stream_->getPrefix());
            
            // register prefix for the stream and RVP
            prefixRegistered_ = false;
            NamespaceInfo ni;
            NameComponents::extractInfo(stream_->getPrefix(), ni);
            s.memCache_->registerPrefix(ni.getPrefix(NameFilter::Library),
                                        [me](const shared_ptr<const Name>& n)
                                        {
                                            me->errorString_ = "Failed to register prefix "+n->toUri();
                                            me->logger_->error("Failed to register prefix {}", n->toUri());
                                        },
                                        [me](const shared_ptr<const Name>& n, uint64_t)
                                        {
                                            me->prefixRegistered_ = true;
                                            me->logger_->info("Registered prefix {}", n->toUri());
                                        });
            
            settings_ = s;
        });
    }
    
    void releaseStream(){
        shared_ptr<spdlog::logger> l = logger_;
        shared_ptr<VideoStream> stream = stream_;
        if (faceProcessor_ && stream)
            faceProcessor_->dispatchSynchronized([stream, l](shared_ptr<Face>)
        {
            // do nothing -- just making sure that stream destructor will be called on face thread
            l->info("Released NDN-RTC stream {}", stream->getPrefix());
        });
        stream.reset();
        cleanupFaceProcessor();
    }

    void publishBgraFrame(const vector<uint8_t>& bgraData, int width, int height){
        allocateYuvData(width, height);

        // using ARGB because of endiannes?
        int res = libyuv::ARGBToI420(bgraData.data(), width*sizeof(uint8_t)*4,
                                     yBuffer(), width,
                                     uBuffer(), width/2,
                                     vBuffer(), width/2,
                                     width, height);
        if (res == 0 && stream_)
        {
            vector<shared_ptr<Data>> packets = stream_->processImage(ImageFormat::I420, yuvData_.data());
            shared_ptr<MemoryContentCache> memCache = settings_.memCache_;
            faceProcessor_->dispatchSynchronized([packets, memCache](shared_ptr<Face> f){
                for (auto& d:packets)
                    memCache->add(*d);
            });
            
            if (packets.size()) NameComponents::extractInfo(packets[0]->getName(), lastFrame_);
        }
    }
    
private:
    shared_ptr<spdlog::logger> logger_;
    string errorString_;
    shared_ptr<helpers::FaceProcessor> faceProcessor_;
    helpers::FaceResetConnection faceResetConnection_;
    VideoStream::Settings settings_;
    shared_ptr<VideoStream> stream_;
    bool prefixRegistered_;
    int width_, height_;
    ndnrtc::NamespaceInfo lastFrame_;
    vector<uint8_t> yuvData_;
    
    void setFaceProcessor(shared_ptr<helpers::FaceProcessor> fp)
    {
        if (faceProcessor_) faceResetConnection_.disconnect();
        
        faceProcessor_ = fp;
        shared_ptr<Impl> me = shared_from_this();
        faceResetConnection_ = faceProcessor_->onFaceReset_.connect([me, fp](const shared_ptr<Face>, const exception&){
            me->stream_.reset();
            me->faceResetConnection_.disconnect();
        });
    }
    
    void cleanupFaceProcessor()
    {
        if (faceProcessor_) faceResetConnection_.disconnect();
        faceProcessor_.reset();
    }
    
    void allocateYuvData(int w, int h){
        size_t len = 3*w*h/2;
        width_ = w; height_ = h;
        
        if (yuvData_.capacity() < len)
            yuvData_.reserve(len);
        
        yuvData_.resize(len);
    }
    
    inline uint8_t* yBuffer() { return (uint8_t*)yuvData_.data(); }
    inline uint8_t* uBuffer() { return yBuffer() + width_*height_; }
    inline uint8_t* vBuffer() { return uBuffer() + width_/2 * ((height_+1) >> 1); }
};

NdnRtcOut::NdnRtcOut(const OP_NodeInfo* info)
: BaseTOP(info)
, useFec_(true)
, dropFrames_(true)
, targetBitrate_(3000)
, segmentSize_(7600)
, gopSize_(30)
, isCacheEnabled_(true)
, bufferWidth_(0)
, bufferHeight_(0)
{
    OPLOG_DEBUG("Create NdnRtcOutTOP");
}

NdnRtcOut::~NdnRtcOut()
{
    OPLOG_DEBUG("Released NdnRtcOutTOP");
}

void
NdnRtcOut::getGeneralInfo(TOP_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1)
{
    ginfo->cookEveryFrameIfAsked = true;
    ginfo->memPixelType = OP_CPUMemPixelType::BGRA8Fixed;
}

bool
NdnRtcOut::getOutputFormat(TOP_OutputFormat *format, const OP_Inputs *inputs, void *reserved1)
{
    return false;
}

void
NdnRtcOut::execute(TOP_OutputFormatSpecs* outputFormat,
                    const OP_Inputs* inputs,
                    TOP_Context *context,
                    void* reserved1)
{
    BaseTOP::execute(outputFormat, inputs, context, reserved1);
    
    if (!pimpl_)
        initStream();
    else
    {
        setError(pimpl_->getErrorString().c_str());
        
        if (inputs->getNumInputs())
        {
            const OP_TOPInput *input = inputs->getInputTOP(0);
            if (input)
            {
                if (input->width > bufferWidth_ ||
                    input->height > bufferHeight_)
                    allocateBuffer(input->width, input->height);
                
//                cout << input->width << "x" << input->height << " "
//                << input->depth << " " << input->pixelFormat << " "
//                << input->textureIndex << endl;
                
//                glBindTexture(GL_TEXTURE_2D, input->textureIndex);
//                GetError()
//                glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA/*input->pixelFormat*//*GL_BGRA*/, GL_UNSIGNED_BYTE, buffer_.data());
//                GetError();
                
                OP_TOPInputDownloadOptions options;
                void *frameData = inputs->getTOPDataInCPUMemory(input, &options);
                if (frameData)
                    memcpy(buffer_.data(), frameData, buffer_.size());

                pimpl_->publishBgraFrame(buffer_, input->width, input->height);
            }
        }
    }
}

void
NdnRtcOut::setupParameters(OP_ParameterManager *manager, void *reserved1)
{
    appendPar<OP_StringParameter>
    (manager, PAR_FACEOP, PAR_FACEOP_LABEL, PAR_PAGE_DEFAULT, [&](OP_StringParameter &p){
        return manager->appendDAT(p);
    });
    
    appendPar<OP_StringParameter>
    (manager, PAR_KEYCHAINOP, PAR_KEYCHAINOP_LABEL, PAR_PAGE_DEFAULT, [&](OP_StringParameter &p){
        return manager->appendDAT(p);
    });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_BITRATE, PAR_BITRATE_LABEL, PAR_PAGE_DEFAULT, [&](OP_NumericParameter &p){
        p.defaultValues[0] = targetBitrate_;
        p.minValues[0] = 100;
        p.minSliders[0] = p.minValues[0];
        p.maxValues[0] = p.defaultValues[0]*10;
        p.maxSliders[0] = p.maxValues[0];
        return manager->appendInt(p);
    });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_SEGSIZE, PAR_SEGSIZE_LABEL, PAR_PAGE_DEFAULT, [&](OP_NumericParameter &p){
        p.defaultValues[0] = segmentSize_;
        p.minValues[0] = 1000;
        p.minSliders[0] = p.minValues[0];
        p.maxValues[0] = 16000;
        p.maxSliders[0] = p.maxValues[0];
        return manager->appendInt(p);
    });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_GOP_SIZE, PAR_GOP_SIZE_LABEL, PAR_PAGE_DEFAULT, [&](OP_NumericParameter &p){
        p.defaultValues[0] = gopSize_;
        p.minValues[0] = 1;
        p.minSliders[0] = p.minValues[0];
        p.maxValues[0] = 300;
        p.maxSliders[0] = p.maxValues[0];
        return manager->appendInt(p);
    });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_USEFEC, PAR_USEFEC_LABEL, PAR_PAGE_DEFAULT, [&](OP_NumericParameter &p){
        p.defaultValues[0] = useFec_;
        return manager->appendToggle(p);
    });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_DROPFRAMES, PAR_DROPFRAMES_LABEL, PAR_PAGE_DEFAULT, [&](OP_NumericParameter &p){
        p.defaultValues[0] = dropFrames_;
        return manager->appendToggle(p);
    });
}

void
NdnRtcOut::checkParams(TOP_OutputFormatSpecs *outputFormat, const OP_Inputs *inputs,
                       TOP_Context *context, void *reserved1)
{
    updateIfNew<string>
    (PAR_FACEOP, faceDat_, getCanonical(inputs->getParString(PAR_FACEOP)),
     [&](string &p){
         return (p != faceDat_) || (getFaceDatOp() == nullptr && p.size());
     });
    
    updateIfNew<string>
    (PAR_KEYCHAINOP, keyChainDat_, getCanonical(inputs->getParString(PAR_KEYCHAINOP)),
     [&](string &p){
         return (p != keyChainDat_) || (getKeyChainDatOp() == nullptr && p.size());
     });
    
    updateIfNew<int>
    (PAR_BITRATE, targetBitrate_, inputs->getParInt(PAR_BITRATE));
    updateIfNew<bool>
    (PAR_USEFEC, useFec_, inputs->getParInt(PAR_USEFEC));
    updateIfNew<bool>
    (PAR_DROPFRAMES, dropFrames_, inputs->getParInt(PAR_DROPFRAMES));
    updateIfNew<int>
    (PAR_SEGSIZE, segmentSize_, inputs->getParInt(PAR_SEGSIZE));
    updateIfNew<int>
    (PAR_GOP_SIZE, gopSize_, inputs->getParInt(PAR_GOP_SIZE));
}

void
NdnRtcOut::paramsUpdated()
{
    runIfUpdated(PAR_FACEOP, [this](){
        dispatchOnExecute([this](TOP_OutputFormatSpecs* outputFormat, const OP_Inputs* inputs,
                                 TOP_Context *context, void* reserved1){
            if (getFaceDatOp()) releaseStream();
            pairOp(faceDat_, true);
        });
    });
    
    runIfUpdated(PAR_KEYCHAINOP, [this](){
        dispatchOnExecute([this](TOP_OutputFormatSpecs* outputFormat, const OP_Inputs* inputs,
                                 TOP_Context *context, void* reserved1){
            if (getKeyChainDatOp()) releaseStream();
            pairOp(keyChainDat_, true);
        });
    });
    
    runIfUpdatedAny({PAR_USEFEC, PAR_BITRATE, PAR_SEGSIZE, PAR_DROPFRAMES}, [this](){
        releaseStream();
    });
}

void
NdnRtcOut::initStream()
{
    if (getFaceDatOp() && getKeyChainDatOp())
    {
        if (getFaceDatOp()->getFaceProcessor() && getKeyChainDatOp()->getKeyChainManager())
        {
            VideoStream::Settings streamSettings = VideoStream::defaultSettings();
            streamSettings.codecSettings_.spec_.encoder_.bitrate_ = targetBitrate_;
            streamSettings.codecSettings_.spec_.encoder_.gop_ = gopSize_;
            streamSettings.codecSettings_.spec_.encoder_.dropFrames_ = dropFrames_;
            streamSettings.useFec_ = useFec_;
            // we'll stuff packets into memcache ourselves
            streamSettings.storeInMemCache_ = false;
            
            clearError();
            pimpl_ = make_shared<Impl>(logger_);
            pimpl_->initStream(BasePrefix, opName_, streamSettings,
                               (isCacheEnabled_ ? cacheLength_ : 1000),
                               getFaceDatOp()->getFaceProcessor(),
                               getKeyChainDatOp()->getKeyChainManager()->instanceKeyChain());
        }
    }
    else
        setError("Face DAT of KeyChain DAT is not specified");
}

void
NdnRtcOut::releaseStream()
{
    pimpl_->releaseStream();
    pimpl_.reset();
}

void
NdnRtcOut::onOpUpdate(OP_Common *op, const std::string &event)
{
    releaseStream();
}

void
NdnRtcOut::opPathUpdated(const std::string &oldFullPath,
                        const std::string &oldOpPath,
                         const std::string &oldOpName)
{
    releaseStream();
}

//******************************************************************************
// InfoDAT and InfoCHOP
const map<NdnRtcOut::InfoChopIndex, string> NdnRtcOut::ChanNames = {
    { NdnRtcOut::InfoChopIndex::FrameNumber, "frameNumber" }
};

const map<NdnRtcOut::InfoDatIndex, string> NdnRtcOut::RowNames = {
    { NdnRtcOut::InfoDatIndex::LibVersion, "Library Version" },
    { NdnRtcOut::InfoDatIndex::StreamPrefix, "Stream Pefix" },
    { NdnRtcOut::InfoDatIndex::FramePrefix, "Frame Pefix" }
};

int32_t
NdnRtcOut::getNumInfoCHOPChans(void* reserved1)
{
    int nStats = 0;
    if (pimpl_ && pimpl_->getIsInitialized())
        nStats += pimpl_->getStats().getIndicators().size();
    return BaseTOP::getNumInfoCHOPChans(reserved1) + (int32_t) ChanNames.size() + nStats;
}

void
NdnRtcOut::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
{
    int nStats = 0;
    if (pimpl_ && pimpl_->getIsInitialized())
    {
        statistics::StatisticsStorage ss = pimpl_->getStats();
        nStats += ss.getIndicators().size();
    }
    
    NdnRtcOut::InfoChopIndex idx = (NdnRtcOut::InfoChopIndex)index;
    
    if (index < ChanNames.size())
    {
        chan->name->setString(ChanNames.at(idx).c_str());
        
        switch (idx) {
            case NdnRtcOut::InfoChopIndex::FrameNumber:
            {
                chan->value = pimpl_ ? pimpl_->getFrameNumber() : -1;
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
    {
        if (pimpl_ && pimpl_->getIsInitialized())
        {
            statistics::StatisticsStorage ss = pimpl_->getStats();
            int nStats = (int)ss.getIndicators().size();
            if (index - ChanNames.size() < nStats)
            {
                int statIdx = ((int)index - (int)ChanNames.size());
                int idx = 0;
                for (auto pair:ss.getIndicators())
                {
                    if (idx == statIdx)
                    {
                        chan->name->setString(statistics::StatisticsStorage::IndicatorKeywords.at(pair.first).c_str());
                        chan->value = (float)pair.second;
                        break;
                    }
                    idx++;
                }
            }
            else
                BaseTOP::getInfoCHOPChan(index - (int32_t)ChanNames.size() - nStats, chan, reserved1);
        }
        else
            BaseTOP::getInfoCHOPChan(index - (int32_t)ChanNames.size(), chan, reserved1);
    }
}

bool
NdnRtcOut::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
    BaseTOP::getInfoDATSize(infoSize, reserved1);
    
    infoSize->rows += RowNames.size();
    
    infoSize->cols = 2;
    infoSize->byColumn = false;
    return true;
}

void
NdnRtcOut::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries* entries,
                           void* reserved1)
{
    size_t nRows = RowNames.size();
    
    if (index < nRows)
    {
        auto idx = (NdnRtcOut::InfoDatIndex)index;
        entries->values[0]->setString(RowNames.at(idx).c_str());
        switch (idx) {
            case NdnRtcOut::InfoDatIndex::LibVersion:
            {
                entries->values[1]->setString("n/a");//ndnrtc_getVersion());
            }
                break;
            case NdnRtcOut::InfoDatIndex::StreamPrefix:
            {
                entries->values[1]->setString(pimpl_ ? pimpl_->getStreamPrefix().c_str() : "n/a");
            }
                break;
            case NdnRtcOut::InfoDatIndex::FramePrefix:
            {
                entries->values[1]->setString(pimpl_ ? pimpl_->getLastFramePrefix().c_str() : "n/a");
            }
                break;
            default:
                entries->values[1]->setString("unknown row index");
                break;
        }
    }
    else
        BaseTOP::getInfoDATEntries((int32_t)(index - nRows), nEntries, entries, reserved1);
}
