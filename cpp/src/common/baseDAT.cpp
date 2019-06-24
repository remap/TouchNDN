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

#include <stdio.h>
#include <string>

#include "baseDAT.hpp"

using namespace std;
using namespace touch_ndn;

BaseDAT::BaseDAT(const OP_NodeInfo* info)
: BaseOp(info)
{
}

BaseDAT::~BaseDAT()
{
    
}

void
BaseDAT::getGeneralInfo(DAT_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1)
{
    ginfo->cookEveryFrame = false;
    ginfo->cookEveryFrameIfAsked = true;
}

void
BaseDAT::execute(DAT_Output *output, const OP_Inputs *inputs, void *reserved1)
{
    try {
        while (executeQueue_.size())
        {
            ExecuteCallback clbck = executeQueue_.front();
            executeQueue_.pop();
            clbck(output, inputs);
        }
    } catch (exception& e) {
        errorString_ = e.what();
    }
}

//int32_t
//BaseDAT::getNumInfoCHOPChans(void *reserved1)
//{
//    return BaseOp::getNumInfoCHOPChans(reserved1);
//}
//
//void
//BaseDAT::getInfoCHOPChan(int32_t index,
//                        OP_InfoCHOPChan *chan,
//                        void* reserved1)
//{
//}
//
//bool
//BaseDAT::getInfoDATSize(OP_InfoDATSize *infoSize, void *reserved1)
//{
//    return false;
//}
//
//void
//BaseDAT::getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries *entries, void *reserved1)
//{
//    
//}
