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

#include "faceDAT-external.hpp"
#include "faceDAT.h"

using namespace touch_ndn;

extern "C"
{
    
    DLLEXPORT
    void
    FillDATPluginInfo(DAT_PluginInfo *info)
    {
        info->apiVersion = DATCPlusPlusAPIVersion;
        info->customOPInfo.opType->setString("Ndnface");
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
