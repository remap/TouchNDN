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

#ifndef baseDAT_hpp
#define baseDAT_hpp

#include <stdio.h>
#include <queue>

#include "baseOP.hpp"
#include "DAT_CPlusPlusBase.h"


namespace ndn {
    class Blob;
}

namespace touch_ndn {
    
    class BaseDAT : public BaseOp<DAT_CPlusPlusBase> {
    public:
        BaseDAT(const OP_NodeInfo* info);
        ~BaseDAT();
        
        virtual void getGeneralInfo(DAT_GeneralInfo* ginfo,
                                    const OP_Inputs* inputs,
                                    void* reserved1) override;
        
        static std::string toBase64(const ndn::Blob&);
        static void fromBase64(const std::string&, std::vector<uint8_t>&);
    protected:
        
    };
}

#endif /* baseDAT_hpp */
