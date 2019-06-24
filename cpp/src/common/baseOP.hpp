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

#ifndef baseOP_hpp
#define baseOP_hpp

#include <stdio.h>
#include <string>

#include "CPlusPlus_Common.h"

namespace touch_ndn {
    
    template<class OP_Base>
    class BaseOp : public OP_Base {
    public:
        BaseOp(const OP_NodeInfo* info)
        : nodeInfo_(info)
        , opName_(extractOpName(info->opPath))
        , errorString_("")
        , warningString_("")
        , executeCount_(0) {}
        
        ~BaseOp(){}
        
        virtual void getWarningString(OP_String *warning, void* reserved1) override
        {
            if (warningString_.size()) warning->setString(warningString_.c_str());
        }
        
        virtual void getErrorString(OP_String *error, void* reserved1) override
        {
            if (errorString_.size()) error->setString(errorString_.c_str());
        }
        
        const OP_NodeInfo*  getNodeInfo() const { return nodeInfo_; }
        
        virtual int32_t
        getNumInfoCHOPChans(void *reserved1) override
        {
            return 2;
        }
        
        virtual void
        getInfoCHOPChan(int32_t index, OP_InfoCHOPChan *chan, void* reserved1) override
        {
            switch (index)
            {
                case 0 :
                {
                    chan->name->setString("executeCount");
                    chan->value = (float)executeCount_;
                } break;
                case 1: {
                    chan->name->setString("executeQueue");
                    chan->value = (float)0;
                } break;
                default: break;
            }
        }
        
        virtual bool
        getInfoDATSize(OP_InfoDATSize *infoSize, void *reserved1) override
        {
            return false;
        }
        
        virtual void
        getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries *entries, void *reserved1) override
        {
            
        }
        
    protected:
        const OP_NodeInfo *nodeInfo_;
        int64_t executeCount_;
        std::string opName_;
        std::string errorString_, warningString_;
        
        std::string extractOpName(std::string opPath) const
        {
            size_t last = 0;
            size_t next = 0;
            
            while ((next = opPath.find("/", last)) != std::string::npos)
                last = next + 1;
            
            return opPath.substr(last);
        }
        
        void setString(std::string &string, const char *format, ...)
        {
            // TBD
        }
        
    };
}

#endif /* baseOP_hpp */
