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

#include "namespaceDAT.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <array>

#include <cnl-cpp/namespace.hpp>

#define PAR_FACEDAT "Facedat"
#define PAR_FACEDAT_LABEL "Face DAT"
#define PAR_KEYCHAINDAT "Keychaindat"
#define PAR_KEYCHAINDAT_LABEL "KeyChain DAT"
#define PAR_FRESHNESS "Freshness"
#define PAR_FRESHNESS_LABEL "Freshness"
#define PAR_HANDLER_TYPE "Handlertype"
#define PAR_HANDLER_TYPE_LABEL "Handler"
#define PAR_TOP_INPUT "Topinput"
#define PAR_TOP_INPUT_LABEL "Payload TOP"

#define PAR_HANDLER_NONE "Handlernone"
#define PAR_HANDLER_NONE_LABEL "None"
#define PAR_HANDLER_SEGMENTED "Handlersegmented"
#define PAR_HANDLER_SEGMENTED_LABEL "Segmented"
#define PAR_HANDLER_GOBJ "Handlergobj"
#define PAR_HANDLER_GOBJ_LABEL "Generalized Object"
#define PAR_HANDLER_GOSTREAM "Handlergobjstream"
#define PAR_HANDLER_GOSTREAM_LABEL "Generalized Object Stream"
#define PAR_OBJECT_NEEDED "Objectneeded"
#define PAR_OBJECT_NEEDED_LABEL "Object Needed"

using namespace std;
using namespace touch_ndn;

extern "C"
{

DLLEXPORT
void
FillDATPluginInfo(DAT_PluginInfo *info)
{
	info->apiVersion = DATCPlusPlusAPIVersion;
	info->customOPInfo.opType->setString("Touchndnnamespace");
	info->customOPInfo.opLabel->setString("Namespace DAT");
	info->customOPInfo.opIcon->setString("NDT");
	info->customOPInfo.authorName->setString("Peter Gusev");
	info->customOPInfo.authorEmail->setString("peter@remap.ucla.edu");
	info->customOPInfo.minInputs = 0;
	info->customOPInfo.maxInputs = 1;
}

DLLEXPORT
DAT_CPlusPlusBase*
CreateDATInstance(const OP_NodeInfo* info)
{
	return new NamespaceDAT(info);
}

DLLEXPORT
void
DestroyDATInstance(DAT_CPlusPlusBase* instance)
{
	delete (NamespaceDAT*)instance;
}

};

NamespaceDAT::NamespaceDAT(const OP_NodeInfo* info)
: BaseDAT(info)
{
}

NamespaceDAT::~NamespaceDAT()
{
}

void
NamespaceDAT::execute(DAT_Output* output,
							const OP_Inputs* inputs,
							void* reserved)
{
    BaseDAT::execute(output, inputs, reserved);
}

void
NamespaceDAT::setupParameters(OP_ParameterManager* manager, void* reserved1)
{
    BaseDAT::setupParameters(manager, reserved1);
    
    appendPar<OP_StringParameter>
    (manager, PAR_FACEDAT, PAR_FACEDAT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
     });
    appendPar<OP_StringParameter>
    (manager, PAR_KEYCHAINDAT, PAR_KEYCHAINDAT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
     });
    appendPar<OP_NumericParameter>
    (manager, PAR_FRESHNESS, PAR_FRESHNESS_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         p.minValues[0] = 0;
         p.maxValues[0] = 24*3600*1000;
         p.minSliders[0] = p.minValues[0];
         p.maxSliders[0] = p.maxValues[0];
         p.defaultValues[0] = freshness_;
         return manager->appendInt(p);
     });
    
#define PAR_HANDLER_MENU_SIZE 4
    static const char *names[PAR_HANDLER_MENU_SIZE] = {
        PAR_HANDLER_NONE,
        PAR_HANDLER_SEGMENTED,
        PAR_HANDLER_GOBJ,
        PAR_HANDLER_GOSTREAM
    };
    static const char *labels[PAR_HANDLER_MENU_SIZE] = {
        PAR_HANDLER_NONE_LABEL,
        PAR_HANDLER_SEGMENTED_LABEL,
        PAR_HANDLER_GOBJ_LABEL,
        PAR_HANDLER_GOSTREAM_LABEL
    };
    
    appendPar<OP_StringParameter>
    (manager, PAR_HANDLER_TYPE, PAR_HANDLER_TYPE_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendMenu(p, PAR_HANDLER_MENU_SIZE, names, labels);
     });
    
    appendPar<OP_NumericParameter>
    (manager, PAR_OBJECT_NEEDED, PAR_OBJECT_NEEDED_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_NumericParameter &p){
         return manager->appendPulse(p);
     });
    
    appendPar<OP_StringParameter>
    (manager, PAR_TOP_INPUT, PAR_TOP_INPUT_LABEL, PAR_PAGE_DEFAULT,
     [&](OP_StringParameter &p){
         return manager->appendDAT(p);
     });
}

void
NamespaceDAT::pulsePressed(const char* name, void* reserved1)
{
    if (string(name) == PAR_OBJECT_NEEDED)
    {
        cout << "object needed " << endl;
    }
    else
        BaseDAT::pulsePressed(name, reserved1);
}
