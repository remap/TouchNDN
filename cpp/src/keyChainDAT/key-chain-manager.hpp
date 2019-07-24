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

#ifndef __keychain_manager_hpp__
#define __keychain_manager_hpp__

#include <string>

namespace ndn {
    class Face;
    class KeyChain;
    class PolicyManager;
    class Data;
    class Name;
    class IdentityStorage;
    class PrivateKeyStorage;
    class MemoryContentCache;
}

namespace touch_ndn
{
namespace helpers
{

    class KeyChainManager {
    public:
        KeyChainManager(std::shared_ptr<ndn::KeyChain> keyChain,
                        const std::string& identityName,
                        const std::string& instanceName,
                        const std::string& configPolicy,
                        unsigned int instanceCertLifetime);
        ~KeyChainManager() {}
        
        std::shared_ptr<ndn::KeyChain> defaultKeyChain() { return defaultKeyChain_; }
        std::shared_ptr<ndn::KeyChain> instanceKeyChain() { return instanceKeyChain_; }
        std::string instancePrefix() const { return instanceIdentity_; }
        
        const std::shared_ptr<ndn::Data> instanceCertificate() const
        { return instanceCert_; }
        
        const std::shared_ptr<ndn::Data> signingIdentityCertificate() const
        { return signingCert_; }
        
        // Will register prefixes for serving instance and signing certificates.
        // Maintains memory content cache for storing certificates.
        // Does not re-publbish certificates.
        void publishCertificates();
        
//        std::shared_ptr<ndn::MemoryContentCache> memoryContentCache() const
//        { return memoryContentCache_; }
        
        // Creates a KeyChain. Depending on provided argument, either system default
        // key chain will be created or file-based keychain will be created.
        // If storagePath != "" then file-base key chain is created. In this case,
        // under storagePath, two items will be created:
        //      - folder "keys" - for storing private and public keys
        //      - file "public-info.db" - for storing pubblic certificates
        // If these files already exist, will create keychain using them.
        static std::shared_ptr<ndn::KeyChain> createKeyChain(std::string storagePath);
        
    private:
//        std::shared_ptr<ndn::Face> face_;
        std::string signingIdentity_, instanceName_,
        configPolicy_, instanceIdentity_;
        unsigned int runTime_;
        
        std::shared_ptr<ndn::PolicyManager> configPolicyManager_;
        std::shared_ptr<ndn::KeyChain> defaultKeyChain_, instanceKeyChain_;
        std::shared_ptr<ndn::Data> instanceCert_, signingCert_;
        std::shared_ptr<ndn::IdentityStorage> identityStorage_;
        std::shared_ptr<ndn::PrivateKeyStorage> privateKeyStorage_;
//        std::shared_ptr<ndn::MemoryContentCache> memoryContentCache_;
        
        void setupDefaultKeyChain();
        void setupInstanceKeyChain();
        void setupConfigPolicyManager();
        void createSigningIdentity();
        void createMemoryKeychain();
        void createInstanceIdentity();
        void createInstanceIdentityV2();
//        void registerPrefix(const ndn::Name& prefix);
        void checkExists(const std::string&);
    };

}
}

#endif
