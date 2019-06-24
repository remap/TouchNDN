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


#include "faceDAT.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <array>

#include <ndn-cpp/face.hpp>

#include "face-processor.hpp"

#define PAR_NFD_HOST "Nfdhost"
#define PAR_NFD_HOST_LABEL "NFD Host"

using namespace std;
using namespace std::placeholders;
using namespace ndn;
using namespace touch_ndn;

extern "C"
{

DLLEXPORT
void
FillDATPluginInfo(DAT_PluginInfo *info)
{
	info->apiVersion = DATCPlusPlusAPIVersion;
	info->customOPInfo.opType->setString("Touchndnface");
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

//******************************************************************************
// InfoDAT and InfoCHOP labels and indexes
const map<FaceDAT::InfoChopIndex, string> FaceDAT::ChanNames = {
    { FaceDAT::InfoChopIndex::FaceProcessing, "faceProcessing" }
};

//******************************************************************************
FaceDAT::FaceDAT(const OP_NodeInfo* info)
: BaseDAT(info)
, nfdHost_("localhost")
{
    dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
}

FaceDAT::~FaceDAT()
{
}

void
FaceDAT::getGeneralInfo(DAT_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved1)
{
	ginfo->cookEveryFrameIfAsked = false;
}

void
FaceDAT::makeTable(DAT_Output* output, int numRows, int numCols)
{
	output->setOutputDataType(DAT_OutDataType::Table);
	output->setTableSize(numRows, numCols);

	std::array<const char*, 5> data = { "this", "is", "some", "test", "data"};

	for (int i = 0; i < numRows; i++)
	{
		for (int j = 0; j < numCols; j++)
		{
			int j2 = j;

			// If we are asked to make more columns than we have data for
			if (j2 >= data.size())
				j2 = j2 % data.size();

			output->setCellString(i, j, data[j2]);
		}
	}
}

void
FaceDAT::makeText(DAT_Output* output)
{
	output->setOutputDataType(DAT_OutDataType::Text);
	output->setText("This is some test data.");
}

void
FaceDAT::execute(DAT_Output* output, const OP_Inputs* inputs, void* reserved)
{
    BaseDAT::execute(output, inputs, reserved);

//    if (!output)
//        return;
//
//    if (inputs->getNumInputs() > 0)
//    {
//        inputs->enablePar("Rows", 0);        // not used
//        inputs->enablePar("Cols", 0);        // not used
//        inputs->enablePar("Outputtype", 0);    // not used
//
//        const OP_DATInput    *cinput = inputs->getInputDAT(0);
//
//        int numRows = cinput->numRows;
//        int numCols = cinput->numCols;
//        bool isTable = cinput->isTable;
//
//        if (!isTable) // is Text
//        {
//            const char* str = cinput->getCell(0, 0);
//            output->setText(str);
//        }
//        else
//        {
//            output->setOutputDataType(DAT_OutDataType::Table);
//            output->setTableSize(numRows, numCols);
//
//            for (int i = 0; i < cinput->numRows; i++)
//            {
//                for (int j = 0; j < cinput->numCols; j++)
//                {
//                    const char* str = cinput->getCell(i, j);
//                    output->setCellString(i, j, str);
//                }
//            }
//        }
//
//    }
//    else // If no input is connected, lets output a custom table/text DAT
//    {
//        inputs->enablePar("Rows", 1);
//        inputs->enablePar("Cols", 1);
//        inputs->enablePar("Outputtype", 1);
//
//        int outputDataType = inputs->getParInt("Outputtype");
//        int     numRows = inputs->getParInt("Rows");
//        int     numCols = inputs->getParInt("Cols");
//
//        switch (outputDataType)
//        {
//            case 0:        // Table
//                makeTable(output, numRows, numCols);
//                break;
//
//            case 1:        // Text
//                makeText(output);
//                break;
//
//            default: // table
//                makeTable(output, numRows, numCols);
//                break;
//        }
//
//        // if there is an input chop parameter:
//        const OP_CHOPInput    *cinput = inputs->getParCHOP("Chop");
//        if (cinput)
//        {
//            int numSamples = cinput->numSamples;
//            int ind = 0;
//            for (int i = 0; i < cinput->numChannels; i++)
//            {
//                myChopChanName = std::string(cinput->getChannelName(i));
//                myChop = inputs->getParString("Chop");
//
//                static char tempBuffer[50];
//                myChopChanVal = float(cinput->getChannelData(i)[ind]);
//
//#ifdef _WIN32
//                sprintf_s(tempBuffer, "%g", myChopChanVal);
//#else // macOS
//                snprintf(tempBuffer, sizeof(tempBuffer), "%g", myChopChanVal);
//#endif
//                if (numCols == 0)
//                    numCols = 2;
//                output->setTableSize(numRows + i + 1, numCols);
//                output->setCellString(numRows + i, 0, myChopChanName.c_str());
//                output->setCellString(numRows + i, 1, &tempBuffer[0]);
//            }
//
//        }
//
//    }
}

int32_t
FaceDAT::getNumInfoCHOPChans(void* reserved1)
{
    return BaseDAT::getNumInfoCHOPChans(reserved1) + (int32_t) ChanNames.size();
}

void
FaceDAT::getInfoCHOPChan(int32_t index, OP_InfoCHOPChan* chan, void* reserved1)
{
    FaceDAT::InfoChopIndex idx = (FaceDAT::InfoChopIndex)index;
    
    if (index < ChanNames.size())
    {
        switch (idx) {
            case FaceDAT::InfoChopIndex::FaceProcessing:
            {
                bool faceProc = faceProcessor_ && faceProcessor_->isProcessing();
                chan->value = (faceProcessor_ && faceProc ? 1. : 0.);
                chan->name->setString(ChanNames.at(idx).c_str());
            }
                break;
                
            default:
                break;
        }
    }
    else
        BaseDAT::getInfoCHOPChan(index - (int32_t)ChanNames.size(), chan, reserved1);
}

bool
FaceDAT::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved1)
{
    BaseDAT::getInfoDATSize(infoSize, reserved1);
    
	infoSize->rows += 0;
	infoSize->cols += 0;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
FaceDAT::getInfoDATEntries(int32_t index,
									int32_t nEntries,
									OP_InfoDATEntries* entries,
									void* reserved1)
{
    BaseDAT::getInfoDATEntries(index, nEntries, entries, reserved1);
}

void
FaceDAT::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
    BaseDAT::setupParameters(manager, reserved1);
    
	{
		OP_StringParameter	np(PAR_NFD_HOST);

		np.label = PAR_NFD_HOST_LABEL;
        np.defaultValue = nfdHost_.c_str();
        np.page = PAR_PAGE_DEFAULT;

		OP_ParAppendResult res = manager->appendString(np);
		assert(res == OP_ParAppendResult::Success);
	}
}

void
FaceDAT::initPulsed()
{
    dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
}

void
FaceDAT::initFace(DAT_Output*, const OP_Inputs* inputs, void* reserved)
{
    faceProcessor_.reset();
    
    try
    {
        std::string hostname(inputs->getParString(PAR_NFD_HOST));
        if (helpers::FaceProcessor::checkNfdConnection(hostname))
        {
            clearError();
            faceProcessor_ = make_shared<helpers::FaceProcessor>(hostname);
            faceProcessor_->start();
        }
        else
            setError("Can't connect to NFD");
    }
    catch (std::runtime_error &e)
    {
        setError("Can't connect to NFD");
    }
}

void
FaceDAT::checkInputs(set<string>& paramNames, DAT_Output *, const OP_Inputs *inputs,
                     void *reserved)
{
    
    if (nfdHost_ != string(inputs->getParString(PAR_NFD_HOST)))
    {
        paramNames.insert(PAR_NFD_HOST);
        nfdHost_ = string(inputs->getParString(PAR_NFD_HOST));
    }
}

void
FaceDAT::paramsUpdated(const std::set<std::string> &updatedParams)
{
    if (updatedParams.find(PAR_NFD_HOST) != updatedParams.end())
    {
        dispatchOnExecute(bind(&FaceDAT::initFace, this, _1, _2, _3));
    }
}


