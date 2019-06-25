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
#include <map>
#include <mutex>
#include <set>

#include "DAT_CPlusPlusBase.h"
#include "baseDAT.hpp"


namespace ndn {
    class Face;
    class Data;
    class Interest;
    class NetworkNack;
}

namespace touch_ndn
{
    
    namespace helpers {
        class FaceProcessor;
    }
    
    class FaceDAT : public BaseDAT
    {
    public:
        enum class InfoChopIndex : int32_t {
            FaceProcessing,
            RequestsTableSize,
            ExpressedNum
        };
        enum class InfoDatIndex : int32_t {
            // nothing
        };
        
        static const std::map<InfoChopIndex, std::string> ChanNames;
        static const std::map<InfoDatIndex, std::string> DatNames;
        
        FaceDAT(const OP_NodeInfo* info);
        virtual ~FaceDAT();
        
        virtual void		getGeneralInfo(DAT_GeneralInfo*, const OP_Inputs*, void* reserved1) override;
        
        virtual void		execute(DAT_Output*, const OP_Inputs*, void* reserved) override;
        
        
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
        virtual void        pulsePressed(const char* name, void* reserved1) override;
        
        std::shared_ptr<helpers::FaceProcessor> getFaceProcessor() const
        { return faceProcessor_; }
        
    private:
        
        const OP_NodeInfo*	myNodeInfo;
        
        void                makeTable(DAT_Output* output, int numRows, int numCols);
        void                makeText(DAT_Output* output);
        
        
        //******************************************************************************
        std::string nfdHost_;
        int32_t lifetime_;
        bool mustBeFresh_, showHeaders_, showFullName_;
        uint32_t nExpressed_;
        std::shared_ptr<helpers::FaceProcessor> faceProcessor_;
        std::set<std::string> currentOutputs_;
        
        typedef struct _RequestStatus {
            _RequestStatus(): isTimeout_(false), isCanceled_(false), pitId_(0),
                expressTs_(0), replyTs_(0) {}
            
            uint64_t pitId_;
            uint32_t expressTs_, replyTs_;
            uint32_t getDrd(){ return replyTs_ - expressTs_;}
            
            bool isTimeout_, isCanceled_;
            std::shared_ptr<const ndn::Interest> interest_;
            std::shared_ptr<ndn::Data> data_;
            std::shared_ptr<ndn::NetworkNack> nack_;
            
            bool isDone() { return data_ || nack_ || isTimeout_; }
        } RequestStatus;

        typedef std::map<std::string, RequestStatus> RequestsDict;
        typedef std::pair<const std::string, RequestStatus> RequestsDictPair;
        typedef struct _RequestsTable {
            std::recursive_mutex mx_;
            RequestsDict dict_;
            
            // mutually acquires access to the table
            void acquire(std::function<void(RequestsDict& d)> f) {
                std::lock_guard<std::recursive_mutex> scopedLock(mx_);
                f(dict_);
            }
            // mutually acquires access to the table if entry, specified by provided interest, existst
            // client callback will be supplied with a copy of the table entry, which should be used
            void acquireIfExists(const ndn::Interest&,
                                 std::function<void(RequestsDict& d, RequestsDict::iterator &it)> f);
            
            bool cancelIfPending(const ndn::Interest&, ndn::Face& f);
            bool setExpressed(const std::shared_ptr<const ndn::Interest>&, uint64_t);
            bool setData(const std::shared_ptr<const ndn::Interest>&, const std::shared_ptr<ndn::Data>&);
            bool setTimeout(const std::shared_ptr<const ndn::Interest>&);
            bool setNack(const std::shared_ptr<const ndn::Interest>&, const std::shared_ptr<ndn::NetworkNack>&);
        } RequestsTable;
        std::shared_ptr<RequestsTable> requestsTable_;
      
        void initPulsed() override;
        void initFace(DAT_Output*, const OP_Inputs*, void* reserved);
        void checkInputs(std::set<std::string>&, DAT_Output*, const OP_Inputs*, void* reserved) override;
        void paramsUpdated(const std::set<std::string>&) override;
        
        void express(const std::vector<std::shared_ptr<ndn::Interest>>&);
        void express(std::string prefix, int lifetime, bool mustBeFresh);
        void express(std::shared_ptr<ndn::Interest>&);
        void cancelRequests();
        
        void setOutputEntry(DAT_Output *output, RequestsDictPair &, int row);
        
      
    };
    
}
