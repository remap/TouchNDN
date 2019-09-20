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

#ifndef ndnrtcIn_hpp
#define ndnrtcIn_hpp

#include <stdio.h>
#include "baseTOP.hpp"

namespace touch_ndn {
    class FaceDAT;
    class KeyChainDAT;
    
    class NdnRtcIn : public BaseTOP {
    public:
        enum class InfoChopIndex : int32_t {
//            FrameNumber
        };
        enum class InfoDatIndex : int32_t {
//            LibVersion,
//            StreamPrefix,
//            FramePrefix
        };
//        static const std::map<InfoChopIndex, std::string> ChanNames;
//        static const std::map<InfoDatIndex, std::string> RowNames;
        
        NdnRtcIn(const OP_NodeInfo* info);
        virtual ~NdnRtcIn();
        
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
        
        virtual int32_t        getNumInfoCHOPChans(void* reserved1) override;
        virtual void        getInfoCHOPChan(int index,
                                            OP_InfoCHOPChan* chan,
                                            void* reserved1) override;
        
        virtual bool        getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1) override;
        virtual void        getInfoDATEntries(int32_t index,
                                              int32_t nEntries,
                                              OP_InfoDATEntries* entries,
                                              void* reserved1) override;
        virtual void initPulsed() override;
    private:
        class Impl;
        std::shared_ptr<Impl> pimpl_;
        int bufferWidth_, bufferHeight_;
        std::vector<uint8_t> buffer_;
        
        bool useFec_, dropFrames_, isCacheEnabled_;
        int32_t pipelineSize_, dqueueSize_;
        std::string faceDat_, keyChainDat_, streamPrefix_;
        
        FaceDAT *getFaceDatOp() { return (FaceDAT*)getPairedOp(faceDat_); }
        KeyChainDAT *getKeyChainDatOp() { return (KeyChainDAT*)getPairedOp(keyChainDat_); }
        
        void initStream();
        void releaseStream();
        
        void onOpUpdate(OP_Common*, const std::string& event) override;
        void opPathUpdated(const std::string& oldFullPath,
                           const std::string& oldOpPath,
                           const std::string& oldOpName) override;
        
        void allocateBuffer(int w, int h)
        {
            bufferWidth_ = w;
            bufferHeight_ = h;
            buffer_ = std::vector<uint8_t>(w*h*4*sizeof(uint8_t));
        }
    };
}

#endif /* ndnrtcOut_hpp */
