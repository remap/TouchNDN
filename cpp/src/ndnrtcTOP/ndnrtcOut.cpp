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
using namespace touch_ndn;

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

NdnRtcOut::NdnRtcOut(const OP_NodeInfo* info)
: BaseTOP(info)
, useFec_(true)
, dropFrames_(true)
, targetBitrate_(3000)
, segmentSize_(7600)
, gopSize_(30)
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
            pairOp(faceDat_, true);
        });
    });
    
    runIfUpdated(PAR_KEYCHAINOP, [this](){
        dispatchOnExecute([this](TOP_OutputFormatSpecs* outputFormat, const OP_Inputs* inputs,
                                 TOP_Context *context, void* reserved1){
            pairOp(keyChainDat_, true);
        });
    });
}
