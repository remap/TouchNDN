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
    class Blob;
}

namespace cnl_cpp {
    class Namespace;
}

namespace touch_ndn
{
    class FaceDAT;
    class KeyChainDAT;
    
/*
 This is a basic sample project to represent the usage of CPlusPlus DAT API.
 To get more help about these functions, look at DAT_CPlusPlusBase.h
*/
class NamespaceDAT : public BaseDAT
{
public:
    enum class HandlerType : int32_t {
        None,
        Segmented,
        GObj,
        GObjStream
    };
    
	NamespaceDAT(const OP_NodeInfo* info);
	virtual ~NamespaceDAT();

    virtual void getGeneralInfo(DAT_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1);
	virtual void execute(DAT_Output*, const OP_Inputs*, void* reserved) override;

//    virtual int32_t        getNumInfoCHOPChans(void* reserved1) override;
//    virtual void        getInfoCHOPChan(int index,
//                                        OP_InfoCHOPChan* chan, 
//                                        void* reserved1) override;
//
    virtual bool        getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1) override;
    virtual void        getInfoDATEntries(int32_t index,
                                            int32_t nEntries,
                                            OP_InfoDATEntries* entries,
                                            void* reserved1) override;

	virtual void		setupParameters(OP_ParameterManager* manager, void* reserved1) override;
	virtual void		pulsePressed(const char* name, void* reserved1) override;

private:
    HandlerType handlerType_;
    uint32_t freshness_;
    std::string prefix_, faceDat_, keyChainDat_, payloadInput_, payloadOutput_;
    FaceDAT *faceDatOp_;
    KeyChainDAT *keyChainDatOp_;
    std::shared_ptr<cnl_cpp::Namespace> namespace_;
    bool rawOutput_, outputSaved_;
    typedef std::vector<std::pair<std::string, std::string>> NamespaceInfoRows;
    std::shared_ptr<NamespaceInfoRows> namespaceInfoRows_;
    std::shared_ptr<bool> prefixRegistered_;
    
    virtual void initPulsed() override;
    virtual void onOpUpdate(OP_Common*, const std::string&) override;
    
    bool isProducer(const OP_Inputs*);
    void checkParams(DAT_Output*, const OP_Inputs*, void* reserved) override;
    void paramsUpdated() override;
  
    void initNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void releaseNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void pairFaceDatOp(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void unpairFaceDatOp(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void pairKeyChainDatOp(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void unpairKeyChainDatOp(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    
    void runPublish(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void runFetch(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void setOutput(DAT_Output *output, const OP_Inputs* inputs, void* reserved);
    void saveOutput(DAT_Output *output, const OP_Inputs* inputs, void* reserved);
    std::shared_ptr<ndn::Blob> getPayload(const OP_Inputs*, std::string& contentType) const;
};

}
