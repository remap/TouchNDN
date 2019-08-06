//
//  face-processor.hpp
//  NDNCon
//
//  Created by Peter Gusev on 2/24/17.
//  Copyright © 2017 UCLA. All rights reserved.
//

#ifndef face_processor_hpp
#define face_processor_hpp

#include <stdio.h>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/asio.hpp>

namespace ndn {
    class Face;
    class Name;
    class Interest;
    class InterestFilter;
}

namespace touch_ndn {
    namespace helpers {
        
        typedef std::function<void
        (const std::shared_ptr<const ndn::Name>& prefix,
        const std::shared_ptr<const ndn::Interest>& interest,
        ndn::Face& face, uint64_t interestFilterId,
        const std::shared_ptr<const ndn::InterestFilter>& filter)> OnInterestCallback;
        
        typedef std::function<void
        (const std::shared_ptr<const ndn::Name>& prefix)> OnRegisterFailed;
        
        typedef std::function<void
        (const std::shared_ptr<const ndn::Name>& prefix,
        uint64_t registeredPrefixId)> OnRegisterSuccess;
        
        class FaceProcessorImpl;
        
        /**
         * FaceProcessor is a thread-safe wrapper around Face or ThreadsafeFace object.
         * Internally, it runs a separate processing thread (that calls processEvents).
         * It provides convenient interface for dispatching callbacks on that thread (either
         * synchronously or asynchronously).
         */
        class FaceProcessor {
        public:
            FaceProcessor(std::string host);
            ~FaceProcessor();
            
            // Starts processing thread
            void start();
            
            // Stops processing thread
            void stop();
            
            // Returns true if processing is running, false otherwise
            bool isProcessing();
            
            // Returns last timestamp when processEvents() was called on the face
            // in milliseconds
            uint64_t getLastProcessingCallTimestamp() const;
            
            // Returns io_service for the processing thread.
            //boost::asio::io_service& getIo();
            
            // Returns face object
            //std::shared_ptr<ndn::Face> getFace();
            
            // Dispatches code block on the face processing thread and returns immediately.
            void dispatchSynchronized(std::function<void(std::shared_ptr<ndn::Face>)> dispatchBlock);
            
            // Dispatches code block on the face processing thread and waits till it gets executed,
            // then returns.
            void performSynchronized(std::function<void(std::shared_ptr<ndn::Face>)> dispatchBlock);
            
            // Registers prefix on the face. All callbacks will be called on the processing thread.
            void registerPrefix(const ndn::Name& prefix,
                                const OnInterestCallback& onInterest,
                                const OnRegisterFailed& onRegisterFailed,
                                const OnRegisterSuccess& onRegisterSuccess);
            
            // Blocking prefix registration - calller will block until either onRegisterFailed or
            // onRegisterSuccess callback is called.
            void registerPrefixBlocking(const ndn::Name& prefix,
                                        const OnInterestCallback& onInterest,
                                        const OnRegisterFailed& onRegisterFailed,
                                        const OnRegisterSuccess& onRegisterSuccess);
            
            
            // Creates FaceProcessor with a Face connected to local NFD
            static std::shared_ptr<FaceProcessor> forLocalhost();
            
            // Checks if NFD is running (tries to register prefix, blocking).
            static bool checkNfdConnection(std::string host = "localhost");
            
        private:
            std::shared_ptr<FaceProcessorImpl> pimpl_;
        };
        
    }
}

#endif /* face_processor_hpp */
