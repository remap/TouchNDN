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
#include <atomic>
#include <mutex>

#include <cnl-cpp/namespace.hpp>
#include "baseDAT.hpp"

namespace ndn {
//    class Object;
    class MetaInfo;
}

namespace cnl_cpp {
    class Namespace;
    class BlobObject;
    class ContentMetaInfoObject;
    class GeneralizedObjectStreamHandler;
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

    virtual void getGeneralInfo(DAT_GeneralInfo *ginfo, const OP_Inputs *inputs, void *reserved1) override;
	virtual void execute(DAT_Output*, const OP_Inputs*, void* reserved) override;
    virtual bool        getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1) override;
    virtual void        getInfoDATEntries(int32_t index,
                                            int32_t nEntries,
                                            OP_InfoDATEntries* entries,
                                            void* reserved1) override;

	virtual void		setupParameters(OP_ParameterManager* manager, void* reserved1) override;
	virtual void		pulsePressed(const char* name, void* reserved1) override;

private:
    uint32_t freshness_;
    std::string prefix_, faceDat_, keyChainDat_, payloadInput_, payloadOutput_;
    bool rawOutput_, payloadStored_, mustBeFresh_, produceOnRequest_, gobjVersioned_;
    std::string outputString_;
    std::vector<std::pair<std::string, std::string>> payloadInfoRows_;
    
    typedef struct _DatInputData {
        std::recursive_mutex mtx_;
        HandlerType handlerType_;
        std::string inputFile_, contentType_;
        std::shared_ptr<ndn::MetaInfo> metaInfo_;
        std::shared_ptr<ndn::Blob> payload_, other_;
    } DatInputData;
    std::shared_ptr<DatInputData> datInputData_;
    
    class Impl;
    std::shared_ptr<Impl> pimpl_;
    
    virtual void initPulsed() override;
    virtual void onOpUpdate(OP_Common*, const std::string&) override;
    
    bool isProducer(const OP_Inputs*);
    void checkParams(DAT_Output*, const OP_Inputs*, void* reserved) override;
    void paramsUpdated() override;
  
    void initNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void releaseNamespace(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    
    FaceDAT *getFaceDatOp() { return (FaceDAT*)getPairedOp(faceDat_); }
    KeyChainDAT *getKeyChainDatOp() { return (KeyChainDAT*)getPairedOp(keyChainDat_); }

    void runPublish(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void runFetch(DAT_Output*output, const OP_Inputs* inputs, void* reserved);
    void setOutput(DAT_Output *output, const OP_Inputs* inputs, void* reserved);
    void storeOutput(DAT_Output *output, const OP_Inputs* inputs, void* reserved);
    
    bool isInputFile(const OP_Inputs* inputs) const;
    // copies payload into Blob(s) so that it can be accessed from another thread
    void copyDatInputData(DAT_Output *output, const OP_Inputs* inputs, void* reserved);
    
    static std::shared_ptr<ndn::Blob> readFile(const std::string& fname, std::string& contentType,
                                               std::shared_ptr<ndn::Blob>& other);
};

}
