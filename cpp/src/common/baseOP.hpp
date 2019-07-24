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

#ifndef baseOP_hpp
#define baseOP_hpp

#include <stdio.h>
#include <string>
#include <queue>
#include <set>

#include "CPlusPlus_Common.h"
#include "touchNDN-shared.hpp"

#define PAR_PAGE_DEFAULT "Custom"
#define PAR_INIT "Init"
#define PAR_INIT_LABEL "Init"

#define OP_EVENT_DESTROY "destroy"

namespace touch_ndn {
    
    namespace helpers {
        // splits a string into an array of token separated by delim
        std::vector<std::string> split(const char *str, std::string delim);
        // canonizes path
        std::string canonical(std::string path);
    }
    
    /**
     * This is a helper class for BaseOP which implements some helper functions.
     */
    class OP_Common {
    public:
        OP_Common();
        virtual ~OP_Common(){ notifyListeners(OP_EVENT_DESTROY); }
        
        void subscribe(OP_Common*);
        void unsubscribe(OP_Common*);
        virtual void onOpUpdate(OP_Common*, const std::string& event){}
        virtual void notifyListeners(const std::string& event);
        
    protected:
        std::vector<OP_Common*> listeners_;
        std::string opName_, opPath_;
        std::string errorString_, warningString_, infoString_;
        
        // extracts operator path (without name) and operator name from full operator path
        void extractOpName(std::string opFullPath, std::string &opPath, std::string &opName);
        
        // checks, whether "path" corresponds to the current opPath_ and opName_. if not, updates
        // them and calls opPathUpdated(...)
        bool opPathChanged(std::string path, std::string& oldPath);
        
        // override in subclasses if needed
        virtual void opPathUpdated(const std::string& oldFullPath,
                                   const std::string& oldOpPath,
                                   const std::string& oldOpName) {}

        void clearError();
        void setError(const char *format, ...);
        void clearWarning();
        void setWarning(const char *format, ...);
        void setInfo(const char *format, ...);
        void setString(std::string &string, const char* format, va_list args);
    };
    
    /**
     * BaseOp is a template class that implements helper functions, common
     * for all TouchDesigner operators (for example, set/get error/warning
     * strings, callback execute queue, etc).
     * It derives from a TouchDesigner's OP class passed as a template
     * parameter.
     */
    template<class OP_Base, class... Arg>
    class BaseOpImpl : public OP_Base, public OP_Common {
    public:
        BaseOpImpl(const OP_NodeInfo* info)
        : nodeInfo_(info)
        , executeCount_(0) {
            extractOpName(info->opPath, opPath_, opName_);
            saveOp(opPath_+opName_, this);
        }
        
        virtual ~BaseOpImpl()
        {
            eraseOp(opPath_+opName_, this);
        }
        
        virtual void getInfoPopupString(OP_String *info, void* reserved1) override
        {
            if (infoString_.size())
                info->setString(infoString_.c_str());
        }
        
        virtual void getWarningString(OP_String *warning, void* reserved1) override
        {
            if (warningString_.size()) warning->setString(warningString_.c_str());
        }
        
        virtual void getErrorString(OP_String *error, void* reserved1) override
        {
            if (errorString_.size()) error->setString(errorString_.c_str());
        }
        
        const OP_NodeInfo*  getNodeInfo() const { return nodeInfo_; }
        
        virtual int32_t
        getNumInfoCHOPChans(void *reserved1) override
        {
            return 2;
        }
        
        virtual void
        getInfoCHOPChan(int32_t index, OP_InfoCHOPChan *chan, void* reserved1) override
        {
            switch (index)
            {
                case 0 :
                {
                    chan->name->setString("executeCount");
                    chan->value = (float)executeCount_;
                } break;
                case 1: {
                    chan->name->setString("executeQueue");
                    chan->value = (float)executeQueue_.size();
                } break;
                default: break;
            }
        }
        
        virtual bool
        getInfoDATSize(OP_InfoDATSize *infoSize, void *reserved1) override
        { return false; }
        
        virtual void
        getInfoDATEntries(int32_t index, int32_t nEntries, OP_InfoDATEntries *entries,
                          void *reserved1) override {}
        
        typedef std::function<void(Arg...)> ExecuteCallback;
        
        virtual void execute(Arg... arg) override
        {
            executeCount_++;
//            clearWarning(); // why clear???
            
            std::string oldFullPath;
            if (opPathChanged(nodeInfo_->opPath, oldFullPath))
            {
                updateOp(oldFullPath, this, opPath_+opName_);
            }
            
            std::set<std::string> updatedParams;

            checkInputs(updatedParams, std::forward<Arg>(arg)...);
            if (updatedParams.size()) paramsUpdated(updatedParams);
            
            // run execute callback queue
            try {
                while (executeQueue_.size())
                {
                    ExecuteCallback c = executeQueue_.front();
                    executeQueue_.pop();
                    c(std::forward<Arg>(arg)...);
                }
            } catch (std::runtime_error &e) {
                setWarning("Caught exception: %s", e.what());
            }
        }
        
        virtual void setupParameters(OP_ParameterManager* manager, void* reserved1) override
        {
            {
                OP_NumericParameter np(PAR_INIT);
                
                np.label = PAR_INIT_LABEL;
                np.page = PAR_PAGE_DEFAULT;
                
                OP_ParAppendResult res = manager->appendPulse(np);
                assert(res == OP_ParAppendResult::Success);
            }
        }
        
        virtual void pulsePressed(const char* name, void* reserved1) override
        {
            if (strcmp(name, PAR_INIT) == 0)
            {
                initPulsed();
            }
        }
        
    protected:
        const OP_NodeInfo *nodeInfo_;
        int64_t executeCount_;
        
        // FIFO Queue of callbbacks that will be called from within execute() method.
        // Queue will be executed until empty.
        // Callbacks should follow certain signature
        std::queue<ExecuteCallback> executeQueue_;
        
        void dispatchOnExecute(ExecuteCallback clbck)
        {
            executeQueue_.push(clbck);
        }
        
        // override in subclasses
        virtual void initPulsed() {}
        // override in subclasses. should add udpated params names into the set
        virtual void checkInputs(std::set<std::string> &updatedParams, Arg... arg) {
            throw std::runtime_error("Not implemented");
        }
        // override in subclasses
        // @param updatedParams Set of names of the updated parameters
        virtual void paramsUpdated(const std::set<std::string> &updatedParams) {
            throw std::runtime_error("Not implemented");
        }
        
        // helper method for setting up operator parameters
        template<class ParameterClass>
        void appendPar(OP_ParameterManager* manager,
                       std::string name, std::string label, std::string page,
                       std::function<OP_ParAppendResult(ParameterClass&)> appendCode)
        {
            ParameterClass p(name.c_str());
            p.label = label.c_str();
            p.page = page.c_str();
            assert(OP_ParAppendResult::Success == appendCode(p));
        }
        
    private:
        
    };
    
    // for details, see
    // https://stackoverflow.com/questions/56742171/how-to-overload-a-method-of-base-class-passed-as-a-parameter-to-a-template-class
    template <class OP_Base, class ExecuteMethod = decltype(&OP_Base::execute)>
    struct BaseOpSelector;
    
    template <class OP_Base, class... Arg>
    struct BaseOpSelector<OP_Base, void (OP_Base::*)(Arg...)>
    {
        using type = BaseOpImpl<OP_Base, Arg...>;
    };
    
    template <class OP_Base>
    using BaseOp = typename BaseOpSelector<OP_Base>::type;
}

#endif /* baseOP_hpp */
