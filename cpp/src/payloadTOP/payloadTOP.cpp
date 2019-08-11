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

#include <iostream>
#include <OpenGL/gl3.h>

#include "payloadTOP.hpp"

#define MODULE_LOGGER "payloadTOP"

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
        info->customOPInfo.opType->setString("Touchndnpayload");
        info->customOPInfo.opLabel->setString("Payload TOP");
        info->customOPInfo.opIcon->setString("NPT");
        info->customOPInfo.authorName->setString("Peter Gusev");
        info->customOPInfo.authorEmail->setString("peter@remap.ucla.edu");
        info->customOPInfo.minInputs = 0;
        info->customOPInfo.maxInputs = 1;
    }
    
    DLLEXPORT
    TOP_CPlusPlusBase*
    CreateTOPInstance(const OP_NodeInfo* info)
    {
        return new PayloadTOP(info);
    }
    
    DLLEXPORT
    void
    DestroyTOPInstance(TOP_CPlusPlusBase* instance)
    {
        delete (PayloadTOP*)instance;
    }
    
};


PayloadTOP::PayloadTOP(const OP_NodeInfo *info)
: BaseTOP(info)
, bufferSize_(0)
, bufferWidth_(0)
, bufferHeight_(0)
, lastBufferUpdate_(0)
{
    OPLOG_DEBUG("Created PayloadTOP");
}

PayloadTOP::~PayloadTOP()
{
    OPLOG_DEBUG("Released PayloadTOP");
}

void
PayloadTOP::getGeneralInfo(TOP_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1)
{
    ginfo->cookEveryFrameIfAsked = true;
    ginfo->memPixelType = OP_CPUMemPixelType::BGRA8Fixed;
}

bool
PayloadTOP::getOutputFormat(TOP_OutputFormat *format, const OP_Inputs *inputs, void *reserved1)
{
    if (inputs->getNumInputs())
    {
        format->width = inputs->getInputTOP(0)->width;
        format->height = inputs->getInputTOP(0)->height;
    }
    else
    {
        format->width = bufferWidth_;
        format->height = bufferHeight_;
    }
    
    return true;
}

void
PayloadTOP::execute(TOP_OutputFormatSpecs* outputFormat,
                      const OP_Inputs* inputs,
                      TOP_Context *context,
                      void* reserved1)
{
    BaseTOP::execute(outputFormat, inputs, context, reserved1);
    
    if (inputs->getNumInputs())
    {
        const OP_TOPInput *input = inputs->getInputTOP(0);
        if (input)
        {
            if (input->width != bufferWidth_ || input->height != bufferHeight_)
                allocateBuffer(input->width, input->height);
            
            glBindTexture(GL_TEXTURE_2D, input->textureIndex);
            GetError()
            glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer_->data());
            GetError();
        }
    }
    
    int textureMemoryLocation = 0;
    uint8_t* mem = (uint8_t*)outputFormat->cpuPixelData[textureMemoryLocation];
    
    if (mem && buffer_)
        memcpy(mem, buffer_->data(), bufferSize_);
    
    outputFormat->newCPUPixelDataLocation = textureMemoryLocation;
    textureMemoryLocation = !textureMemoryLocation;
}
