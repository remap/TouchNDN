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

#include <string>

#include "baseDAT.hpp"

namespace ndn {
    class KeyChain;
    class Face;
}

namespace touch_ndn {
    
    namespace helpers {
        class KeyChainManager;
    }
    
    class KeyChainDAT : public BaseDAT
    {
    public:
        enum class KeyChainType : int32_t {
            System,
            File,
            Embedded
        };
        
        KeyChainDAT(const OP_NodeInfo* info);
        virtual ~KeyChainDAT();
        
        virtual void		execute(DAT_Output*,
                                    const OP_Inputs*,
                                    void* reserved) override;
        
        virtual void        getGeneralInfo(DAT_GeneralInfo *ginfo,
                                           const OP_Inputs *inputs, void *reserved1) override;
        
        virtual int32_t		getNumInfoCHOPChans(void* reserved1) override;
        virtual void		getInfoCHOPChan(int index,
                                            OP_InfoCHOPChan* chan,
                                            void* reserved1) override;
        
        virtual bool		getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1) override;
        virtual void		getInfoDATEntries(int32_t index,
                                              int32_t nEntries,
                                              OP_InfoDATEntries* entries,
                                              void* reserved1) override;
        
        virtual void		setupParameters(OP_ParameterManager* manager, void* reserved1) override;
        
        std::shared_ptr<helpers::KeyChainManager> getKeyChainManager() { return keyChainManager_; }
        
    private:
        KeyChainType keyChainType_;
        std::shared_ptr<helpers::KeyChainManager> keyChainManager_;
        
        void onOpUpdate(OP_Common*, const std::string&);
        
        void initPulsed() override;
        void checkParams(DAT_Output*, const OP_Inputs*, void* reserved) override;
        void paramsUpdated() override;
        
        void initKeyChain(DAT_Output*, const OP_Inputs*, void* reserved);
    };
}
