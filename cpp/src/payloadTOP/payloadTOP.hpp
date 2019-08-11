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

#ifndef payloadTOP_hpp
#define payloadTOP_hpp

#include <stdio.h>
#include "baseTOP.hpp"

namespace touch_ndn {
    class PayloadTOP : public BaseTOP {
    public:
        PayloadTOP(const OP_NodeInfo* info);
        virtual ~PayloadTOP();
        
        virtual void getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs* inputs,
                                    void* reserved1) override;
        bool getOutputFormat(TOP_OutputFormat* format, const OP_Inputs* inputs,
                             void* reserved1) override;
        virtual void execute(TOP_OutputFormatSpecs* outputFormat, const OP_Inputs* inputs,
                             TOP_Context *context, void* reserved1) override;
        
        virtual void checkParams(TOP_OutputFormatSpecs* outputFormat,
                                 const OP_Inputs* inputs,
                                 TOP_Context *context,
                                 void* reserved1) override {}
        virtual void paramsUpdated() override {}
        
        std::shared_ptr<std::vector<uint8_t>> getBuffer(int &size, int &width, int &height) const
        {
            size = bufferSize_;
            width = bufferWidth_;
            height = bufferHeight_;
            return buffer_;
        }
        
        void setBuffer(const std::vector<uint8_t> &buffer, int width, int height)
        {
            allocateBuffer(width, height);
            assert(bufferSize_ == buffer.size());
            memcpy(buffer_->data(), buffer.data(), bufferSize_);
        }

    private:
        int bufferSize_, bufferWidth_, bufferHeight_;
        uint64_t lastBufferUpdate_;
        std::shared_ptr<std::vector<uint8_t>> buffer_;
        
        void allocateBuffer(int w, int h)
        {
            bufferWidth_ = w;
            bufferHeight_ = h;
            bufferSize_ = w*h*4*sizeof(uint8_t);
            buffer_ = std::make_shared<std::vector<uint8_t>>(bufferSize_);
        }
    };
}

#endif /* payloadTOP_hpp */
