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

#ifndef ndnrtcOut_hpp
#define ndnrtcOut_hpp

#include <stdio.h>
#include "baseTOP.hpp"

namespace ndnrtc {
    class VideoStream;
}

namespace touch_ndn {
    class FaceDAT;
    class KeyChainDAT;
    
    class NdnRtcOut : public BaseTOP {
    public:
        NdnRtcOut(const OP_NodeInfo* info);
        virtual ~NdnRtcOut();
        
        virtual void getGeneralInfo(TOP_GeneralInfo* ginfo, const OP_Inputs* inputs,
                                    void* reserved1) override;
        bool getOutputFormat(TOP_OutputFormat* format, const OP_Inputs* inputs,
                             void* reserved1) override;
        virtual void execute(TOP_OutputFormatSpecs* outputFormat, const OP_Inputs* inputs,
                             TOP_Context *context, void* reserved1) override;
        
        virtual void setupParameters(OP_ParameterManager* manager, void* reserved1) override;
        virtual void checkParams(TOP_OutputFormatSpecs* outputFormat,
                                 const OP_Inputs* inputs,
                                 TOP_Context *context,
                                 void* reserved1) override;
        virtual void paramsUpdated() override;
        
    private:
        bool useFec_, dropFrames_;
        int32_t targetBitrate_, segmentSize_, gopSize_;
        std::string faceDat_, keyChainDat_;
        std::shared_ptr<ndnrtc::VideoStream> stream_;
        
        FaceDAT *getFaceDatOp() { return (FaceDAT*)getPairedOp(faceDat_); }
        KeyChainDAT *getKeyChainDatOp() { return (KeyChainDAT*)getPairedOp(keyChainDat_); }
        
        void initStream();
    };
}

#endif /* ndnrtcOut_hpp */
