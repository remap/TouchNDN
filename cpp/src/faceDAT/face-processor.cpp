//
//  face-processor.cpp
//  NDNCon
//
//  Created by Peter Gusev on 2/24/17.
//  Copyright © 2017 UCLA. All rights reserved.
//

#include "face-processor.hpp"

#include <thread>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <atomic>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/asio/io_service.hpp>

#include <ndn-cpp/threadsafe-face.hpp>
#include <ndn-cpp/face.hpp>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/pib/pib-memory.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-memory.hpp>

#define USE_THREADSAFE_FACE

using namespace ndn;
using namespace std;
using namespace boost::asio;
using namespace touch_ndn::helpers;

namespace touch_ndn {
    namespace helpers {
        class FaceProcessorImpl : public enable_shared_from_this<FaceProcessorImpl>{
        public:
            FaceProcessorImpl(std::string host);
            ~FaceProcessorImpl();

            void start();
            void stop();
            bool isProcessing();

            // non blocking
            void dispatchSynchronized(function<void(shared_ptr<ndn::Face>)> dispatchBlock);
            // blocking
            void performSynchronized(function<void(shared_ptr<ndn::Face>)> dispatchBlock);

            bool initFace();
            void runFace();

            io_service& getIo() { return io_; }
            shared_ptr<Face> getFace() { return face_; }

        private:
            std::string host_;
            shared_ptr<Face> face_;
            thread t_;
            bool isRunningFace_;
            io_service io_;
#ifdef USE_THREADSAFE_FACE
            shared_ptr<io_service::work> ioWork_;
#endif
        };
    }
}

shared_ptr<FaceProcessor>
FaceProcessor::forLocalhost()
{
    return make_shared<FaceProcessor>("localhost");
}
bool FaceProcessor::checkNfdConnection(string host)
{
    bool done = false;
    uint64_t registeredPrefixId = 0;
    auto t = chrono::steady_clock::now();
    double timeout = (host == "localhost" ? 500 : 2000);
    
#if 0
    KeyChain keyChain;
#else
    KeyChain keyChain(make_shared<PibMemory>(), make_shared<TpmBackEndMemory>());
    keyChain.createIdentityAndCertificate("connectivity-check");
#endif
    Face face(host.c_str());
    
    face.setCommandSigningInfo(keyChain, keyChain.getDefaultCertificateName());
    face.registerPrefix(ndn::Name("/nfd-connectivity-check"),
                        [](const shared_ptr<const ndn::Name>& prefix,
                                                    const shared_ptr<const ndn::Interest>& interest, ndn::Face& face,
                                                    uint64_t interestFilterId,
                                                    const shared_ptr<const ndn::InterestFilter>& filter){},
                        [&done](const shared_ptr<const ndn::Name>& prefix){
                            // failure
                            done = true;
                        },
                        [&done,&registeredPrefixId](const shared_ptr<const ndn::Name>& prefix,
                                                    uint64_t prefId){
                            // success
                            registeredPrefixId = prefId;
                            done = true;
                        });

    double d = 0;
    do
    {
        face.processEvents();
        d = (double)chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - t).count();
    } while (!done && d < timeout);
    
    if (registeredPrefixId) face.removeRegisteredPrefix(registeredPrefixId);
    
    return (registeredPrefixId != 0);
}

FaceProcessor::FaceProcessor(std::string host):
pimpl_(make_shared<FaceProcessorImpl>(host))
{
    if (pimpl_->initFace())
        pimpl_->runFace();
    else
        throw std::runtime_error("couldn't initialize face object");
}

FaceProcessor::~FaceProcessor() {
    pimpl_->stop();
    pimpl_.reset();
}

void FaceProcessor::start() { pimpl_->start(); }
void FaceProcessor::stop() { pimpl_->stop(); }
bool FaceProcessor::isProcessing() { return pimpl_->isProcessing(); }
void FaceProcessor::dispatchSynchronized(function<void (shared_ptr<Face>)> dispatchBlock)
{
    return pimpl_->dispatchSynchronized(dispatchBlock);
}
void FaceProcessor::performSynchronized(function<void (shared_ptr<Face>)> dispatchBlock)
{
    return pimpl_->performSynchronized(dispatchBlock);
}
//boost::asio::io_service& FaceProcessor::getIo() { return pimpl_->getIo(); }
//shared_ptr<Face> FaceProcessor::getFace() { return pimpl_->getFace(); }

void FaceProcessor::registerPrefix(const Name& prefix, 
                                   const OnInterestCallback& onInterest,
                                   const OnRegisterFailed& onRegisterFailed,
                                   const OnRegisterSuccess& onRegisterSuccess)
{
    pimpl_->performSynchronized([prefix, onInterest, onRegisterFailed, onRegisterSuccess](shared_ptr<ndn::Face> face){
        face->registerPrefix(prefix, onInterest, onRegisterFailed, onRegisterSuccess);
    });
}

void FaceProcessor::registerPrefixBlocking(const ndn::Name& prefix, 
                const OnInterestCallback& onInterest,
                const OnRegisterFailed& onRegisterFailed,
                const OnRegisterSuccess& onRegisterSuccess)
{
    mutex m;
    unique_lock<mutex> lock(m);
    condition_variable isDone;
    atomic<bool> completed(false);
    bool registered = false;

    pimpl_->dispatchSynchronized([prefix, onInterest, onRegisterFailed, onRegisterSuccess](shared_ptr<ndn::Face> face){
        face->registerPrefix(prefix, onInterest, onRegisterFailed, onRegisterSuccess);
    });

    isDone.wait(lock, [&completed](){ return completed.load(); });
}

//******************************************************************************
FaceProcessorImpl::FaceProcessorImpl(std::string host):host_(host), isRunningFace_(false)
{
}

FaceProcessorImpl::~FaceProcessorImpl()
{
    stop();
}

void FaceProcessorImpl::start()
{
    if (!isRunningFace_)
        if (initFace())
            runFace();
}

void FaceProcessorImpl::stop()
{
    if (isRunningFace_)
    {
        isRunningFace_ = false;
        
#ifdef USE_THREADSAFE_FACE
        face_->shutdown();
        ioWork_.reset();
        io_.stop();
        t_.join();
#else
//        std::cout << "t join" << std::endl;
        t_.join();
        io_.stop();
        face_->shutdown();
#endif
//        std::cout << "stopped" << std::endl;
    }
}

bool FaceProcessorImpl::isProcessing()
{
    return isRunningFace_;
}

void FaceProcessorImpl::dispatchSynchronized(function<void (shared_ptr<ndn::Face>)> dispatchBlock)
{
    if (isRunningFace_)
    {
        shared_ptr<Face> f = face_;
        io_.dispatch([dispatchBlock, f](){
            dispatchBlock(f);
        });
    }
}

void FaceProcessorImpl::performSynchronized(function<void (shared_ptr<ndn::Face>)> dispatchBlock)
{
    if (isRunningFace_)
    {
        if (this_thread::get_id() == t_.get_id())
            dispatchBlock(face_);
        else
        {
            mutex m;
            unique_lock<mutex> lock(m);
            condition_variable isDone;
            atomic<bool> doneFlag(false);
            shared_ptr<Face> face = face_;
            
            io_.dispatch([dispatchBlock, face, &isDone, &doneFlag](){
                dispatchBlock(face);
                doneFlag = true;
                isDone.notify_one();
            });
            
            isDone.wait(lock, [&doneFlag](){ return doneFlag.load(); });
        }
    }
}

bool FaceProcessorImpl::initFace()
{
    try {
        if (host_ == "localhost")
#ifdef USE_THREADSAFE_FACE
            face_ = make_shared<ThreadsafeFace>(io_);
#else
            face_ = make_shared<Face>();
#endif
        else
#ifdef USE_THREADSAFE_FACE
            face_ = make_shared<ThreadsafeFace>(io_, host_.c_str());
#else
            face_ = make_shared<Face>(host_.c_str());
#endif
    }
    catch(std::exception &e)
    {
        // notify about error
        return false;
    }
    
    return true;
}

void FaceProcessorImpl::runFace()
{
#ifdef USE_THREADSAFE_FACE
    ioWork_ = make_shared<io_service::work>(io_);
#endif
    isRunningFace_ = false;
    
    shared_ptr<FaceProcessorImpl> self = shared_from_this();
    
    t_ = thread([self](){
        self->isRunningFace_ = true;
        while (self->isRunningFace_)
        {
            try {
#ifdef USE_THREADSAFE_FACE
                self->io_.run();
                self->isRunningFace_ = false;
#else
                self->io_.poll_one();
                self->io_.reset();
                self->face_->processEvents();
#endif
            }
            catch (std::exception &e) {
                // notify about error and try to recover
                if (!self->initFace())
                    self->isRunningFace_ = false;
            }
        }
    });
    
    while (!isRunningFace_) usleep(10000);
}
