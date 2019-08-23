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

#include <OpenGL/gl3.h>
#include <ndnrtc/stream.hpp>
#include <ndnrtc/name-components.hpp>
#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>

#include "faceDAT.h"
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
    
    string getErrorString() const { return errorString_; }
    
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
                                        },
                                        [me](const shared_ptr<const Name>& n, uint64_t)
                                        {
                                            me->prefixRegistered_ = true;
                                            me->logger_->info("Registered prefix {}", n->toUri());
                                        });
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

    
private:
    shared_ptr<spdlog::logger> logger_;
    string errorString_;
    shared_ptr<helpers::FaceProcessor> faceProcessor_;
    helpers::FaceResetConnection faceResetConnection_;
    shared_ptr<VideoStream> stream_;
    bool prefixRegistered_;
    
    
    void setFaceProcessor(shared_ptr<helpers::FaceProcessor> fp)
    {
        if (faceProcessor_) faceResetConnection_.disconnect();
        
        faceProcessor_ = fp;
        shared_ptr<Impl> me = shared_from_this();
        faceProcessor_->onFaceReset_.connect([me, fp](const shared_ptr<Face>, const exception&){
            me->stream_.reset();
            me->faceResetConnection_.disconnect();
        });
    }
    
    void cleanupFaceProcessor()
    {
        if (faceProcessor_) faceResetConnection_.disconnect();
        faceProcessor_.reset();
    }
};

NdnRtcOut::NdnRtcOut(const OP_NodeInfo* info)
: BaseTOP(info)
, useFec_(true)
, dropFrames_(true)
, targetBitrate_(3000)
, segmentSize_(7600)
, gopSize_(30)
, isCacheEnabled_(true)
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
    return true;
}

void
NdnRtcOut::execute(TOP_OutputFormatSpecs* outputFormat,
                    const OP_Inputs* inputs,
                    TOP_Context *context,
                    void* reserved1)
{
    BaseTOP::execute(outputFormat, inputs, context, reserved1);
    
    if (!pimpl_) initStream();
    else
    {
        setError(pimpl_->getErrorString().c_str());
        
        //...
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
        VideoStream::Settings streamSettings = VideoStream::defaultSettings();
        streamSettings.codecSettings_.spec_.encoder_.bitrate_ = targetBitrate_;
        streamSettings.codecSettings_.spec_.encoder_.gop_ = gopSize_;
        streamSettings.codecSettings_.spec_.encoder_.dropFrames_ = dropFrames_;
        streamSettings.useFec_ = useFec_;
        streamSettings.storeInMemCache_ = isCacheEnabled_;
        
        clearError();
        pimpl_ = make_shared<Impl>(logger_);
        pimpl_->initStream(BasePrefix, opName_, streamSettings,
                           (isCacheEnabled_ ? cacheLength_ : 0),
                           getFaceDatOp()->getFaceProcessor(),
                           getKeyChainDatOp()->getKeyChainManager()->instanceKeyChain());
    }
    else
        setError("Face of KeyChain is not specified");
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
