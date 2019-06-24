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

#include "DAT_CPlusPlusBase.h"
#include "baseDAT.hpp"


namespace touch_ndn
{
    
    namespace helpers {
        class FaceProcessor;
    }
    
    class FaceDAT : public BaseDAT
    {
    public:
        FaceDAT(const OP_NodeInfo* info);
        virtual ~FaceDAT();
        
        virtual void		getGeneralInfo(DAT_GeneralInfo*, const OP_Inputs*, void* reserved1) override;
        
        virtual void		execute(DAT_Output*,
                                    const OP_Inputs*,
                                    void* reserved) override;
        
        
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
        virtual void		pulsePressed(const char* name, void* reserved1) override;
        
    private:
        
        void				makeTable(DAT_Output* output, int numRows, int numCols);
        void				makeText(DAT_Output* output);
        
        // We don't need to store this pointer, but we do for the example.
        // The OP_NodeInfo class store information about the node that's using
        // this instance of the class (like its name).
        const OP_NodeInfo*	myNodeInfo;
        
        // In this example this value will be incremented each time the execute()
        // function is called, then passes back to the DAT
        int32_t				myExecuteCount;
        
        double				myOffset;
        
        std::string         myChopChanName;
        float               myChopChanVal;
        std::string         myChop;
        
        std::string         myDat;
        
        
        //******************************************************************************
        std::shared_ptr<helpers::FaceProcessor> faceProcessor_;
      
      
    };
    
}
