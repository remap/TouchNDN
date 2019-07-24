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

#include "key-chain-manager.hpp"

#include <algorithm>
#include <fstream>
#include <chrono>
#include <ndn-cpp/security/key-chain.hpp>
#include <ndn-cpp/security/certificate/identity-certificate.hpp>
#include <ndn-cpp/security/identity/memory-identity-storage.hpp>
#include <ndn-cpp/security/identity/memory-private-key-storage.hpp>
#include <ndn-cpp/security/identity/file-private-key-storage.hpp>
#include <ndn-cpp/security/identity/basic-identity-storage.hpp>
#include <ndn-cpp/security/policy/config-policy-manager.hpp>
#include <ndn-cpp/security/policy/self-verify-policy-manager.hpp>
#include <ndn-cpp/security/v2/certificate-cache-v2.hpp>
#include <ndn-cpp/security/pib/pib-memory.hpp>
#include <ndn-cpp/security/pib/pib-sqlite3.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-memory.hpp>
#include <ndn-cpp/security/tpm/tpm-back-end-file.hpp>
#include <ndn-cpp/security/signing-info.hpp>
#include <ndn-cpp/util/memory-content-cache.hpp>
#include <ndn-cpp/face.hpp>

using namespace std;
using namespace std::chrono;
using namespace ndn;
using namespace touch_ndn::helpers;

static const char *PublicDb = "pib.db";
static const char *PrivateDb = "ndnsec-key-file";

shared_ptr<KeyChain>
KeyChainManager::createKeyChain(string storagePath)
{
	shared_ptr<KeyChain> keyChain;

	if (storagePath.size())
	{
		string privateKeysPath = string(storagePath) + "/" + string(PrivateDb);

#if SECURITY_V1
        string databaseFilePath = string(storagePath) + "/" + string(PublicDb);
		shared_ptr<IdentityStorage> identityStorage = make_shared<BasicIdentityStorage>(databaseFilePath);
		shared_ptr<IdentityManager> identityManager = make_shared<IdentityManager>(identityStorage,
			make_shared<FilePrivateKeyStorage>(privateKeysPath));
		shared_ptr<PolicyManager> policyManager = make_shared<SelfVerifyPolicyManager>(identityStorage.get());

		keyChain = make_shared<KeyChain>(identityManager, policyManager);
#else
        string databaseFilePath = string(storagePath);
        shared_ptr<PibSqlite3> pib = make_shared<PibSqlite3>(databaseFilePath);
        // pib->setTpmLocator();
        shared_ptr<TpmBackEndFile> tpm = make_shared<TpmBackEndFile>(privateKeysPath);

        keyChain = make_shared<KeyChain>(pib, tpm);
#endif
	}
	else
	{
		keyChain = make_shared<KeyChain>();
	}

	return keyChain;
}

KeyChainManager::KeyChainManager(shared_ptr<KeyChain> keyChain,
                    const string& identityName,
                    const string& instanceName,
                    const string& configPolicy,
                    unsigned int instanceCertLifetime):
defaultKeyChain_(keyChain),
signingIdentity_(identityName),
instanceName_(instanceName),
configPolicy_(configPolicy),
runTime_(instanceCertLifetime)
{
	if (configPolicy != "")
	{
		cout << "Setting up file-based policy manager from " << configPolicy << endl;

		checkExists(configPolicy);
		setupConfigPolicyManager();
	}
	else
	{
		cout << "Setting self-verify policy manager..." << endl;

		identityStorage_ = make_shared<MemoryIdentityStorage>();
		privateKeyStorage_ = make_shared<MemoryPrivateKeyStorage>();
		configPolicyManager_ = make_shared<SelfVerifyPolicyManager>(identityStorage_.get());
	}

	setupInstanceKeyChain();
}

//void KeyChainManager::publishCertificates()
//{
//    memoryContentCache_ = make_shared<MemoryContentCache>(face_.get());
//
//    { // registering prefix for serving signing identity: <signing-identity>/KEY
//        Name prefix(signingIdentity_);
//        prefix.append("KEY");
//        registerPrefix(prefix);
//    }
//
//    { // registering prefix for serving instance identity: <instance-identity>/KEY
//        Name prefix(signingIdentity_);
//        prefix.append(instanceName_).append("KEY");
//        registerPrefix(prefix);
//    }
//
//    memoryContentCache_->add(*(this->signingIdentityCertificate()));
//    memoryContentCache_->add(*(this->instanceCertificate()));
//}

//******************************************************************************
void KeyChainManager::setupDefaultKeyChain()
{
    defaultKeyChain_ = make_shared<KeyChain>();
}

void KeyChainManager::setupInstanceKeyChain()
{
    Name signingIdentity(signingIdentity_);
    vector<Name> identities;

    if (defaultKeyChain_->getIsSecurityV1())
    {
        defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, false);
        defaultKeyChain_->getIdentityManager()->getAllIdentities(identities, true);
    }
    else
        defaultKeyChain_->getPib().getAllIdentityNames(identities);

    if (find(identities.begin(), identities.end(), signingIdentity) == identities.end())
    {
        // create signing identity in default keychain
        cout << "Signing identity not found. Creating..." << endl;
        createSigningIdentity();
    }

	if (defaultKeyChain_->getIsSecurityV1()) {
		Name certName = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(signingIdentity);
		signingCert_ = defaultKeyChain_->getCertificate(certName);
	}
	else
	{
		Name certName = defaultKeyChain_->getPib().getIdentity(signingIdentity)->getDefaultKey()->getDefaultCertificate()->getName();
		signingCert_ = defaultKeyChain_->getPib().getIdentity(CertificateV2::extractIdentityFromCertName(certName))
				->getKey(CertificateV2::extractKeyNameFromCertName(certName))
    			->getCertificate(certName);
	}

//    face_->setCommandSigningInfo(*defaultKeyChain_, signingCert_->getName());

	createMemoryKeychain();
	if (defaultKeyChain_->getIsSecurityV1())
		createInstanceIdentity();
	else
		createInstanceIdentityV2();
}

void KeyChainManager::setupConfigPolicyManager()
{
    identityStorage_ = make_shared<MemoryIdentityStorage>();
    privateKeyStorage_ = make_shared<MemoryPrivateKeyStorage>();
    if (defaultKeyChain_->getIsSecurityV1())
    {
        configPolicyManager_ = make_shared<ConfigPolicyManager>(configPolicy_);
    }
    else
    {
        configPolicyManager_ = make_shared<ConfigPolicyManager>(configPolicy_, make_shared<CertificateCacheV2>());
    }
}

void KeyChainManager::createSigningIdentity()
{
    // create self-signed certificate
    Name cert = defaultKeyChain_->createIdentityAndCertificate(Name(signingIdentity_));

	cout << "Generated identity " << signingIdentity_ << " (certificate name "
		<< cert << ")" << endl;
	cout << "Check policy config file for correct trust anchor (run `ndnsec-dump-certificate -i "
		<< signingIdentity_  << " > signing.cert` if needed)" << endl;
}

void KeyChainManager::createMemoryKeychain()
{
    if (defaultKeyChain_->getIsSecurityV1())
        instanceKeyChain_ = make_shared<KeyChain>(make_shared<IdentityManager>(identityStorage_, privateKeyStorage_),
                                                         configPolicyManager_);
    else
        instanceKeyChain_ = make_shared<KeyChain>(make_shared<PibMemory>(), make_shared<TpmBackEndMemory>(),
                                                         configPolicyManager_);

    // TODO: setup face
//    instanceKeyChain_->setFace(face_.get());
}

void KeyChainManager::createInstanceIdentity()
{
    auto now = system_clock::now().time_since_epoch();
    duration<double> sec = now;

    Name instanceIdentity(signingIdentity_);

    instanceIdentity.append(instanceName_);
    instanceIdentity_ = instanceIdentity.toUri();

    cout << "Instance identity " << instanceIdentity << endl;

    Name instanceKeyName = instanceKeyChain_->generateRSAKeyPairAsDefault(instanceIdentity, true);
    Name signingCert = defaultKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(signingIdentity_));

	cout << "Instance key " << instanceKeyName << endl;
	cout << "Signing certificate " << signingCert << endl;

    vector<CertificateSubjectDescription> subjectDescriptions;
    instanceCert_ =
        instanceKeyChain_->getIdentityManager()->prepareUnsignedIdentityCertificate(instanceKeyName,
                                                                                    Name(signingIdentity_),
                                                                                    sec.count() * 1000,
                                                                                    (sec + duration<double>(runTime_)).count() * 1000,
                                                                                    subjectDescriptions,
                                                                                    &instanceIdentity);
    assert(instanceCert_.get());

    defaultKeyChain_->sign(*instanceCert_, signingCert);
    instanceKeyChain_->installIdentityCertificate(*instanceCert_);
    instanceKeyChain_->setDefaultCertificateForKey(*instanceCert_);
    instanceKeyChain_->getIdentityManager()->setDefaultIdentity(instanceIdentity);

	cout << "Instance certificate "
		<< instanceKeyChain_->getIdentityManager()->getDefaultCertificateNameForIdentity(Name(instanceIdentity)) << endl;
}

void KeyChainManager::createInstanceIdentityV2()
{
    auto now = system_clock::now().time_since_epoch();
    duration<double> sec = now;

    Name instanceIdentity(signingIdentity_);

    instanceIdentity.append(instanceName_);
    instanceIdentity_ = instanceIdentity.toUri();

    cout << "Instance identity " << instanceIdentity << endl;

	shared_ptr<PibIdentity> instancePibIdentity =
      instanceKeyChain_->createIdentityV2(instanceIdentity);
	shared_ptr<PibKey> instancePibKey =
      instancePibIdentity->getDefaultKey();
	shared_ptr<PibKey> signingPibKey = defaultKeyChain_->getPib()
      .getIdentity(Name(signingIdentity_))->getDefaultKey();
	Name signingCert = signingPibKey->getDefaultCertificate()->getName();

	cout << "Instance key " << instancePibKey->getName() << endl;
	cout << "Signing certificate " << signingCert << endl;

    // Prepare the instance certificate.
    shared_ptr<CertificateV2> instanceCertificate(new CertificateV2());
    Name certificateName = instancePibKey->getName();
    // Use the issuer's public key digest.
    certificateName.append(PublicKey(signingPibKey->getPublicKey()).getDigest());
    certificateName.appendVersion((uint64_t)(sec.count() * 1000));
    instanceCertificate->setName(certificateName);
    instanceCertificate->getMetaInfo().setType(ndn_ContentType_KEY);
    instanceCertificate->getMetaInfo().setFreshnessPeriod(3600 * 1000.0);
    instanceCertificate->setContent(instancePibKey->getPublicKey());

    SigningInfo signingParams(signingPibKey);
    signingParams.setValidityPeriod(ValidityPeriod(sec.count() * 1000,
                                                   (sec + duration<double>(runTime_)).count() * 1000));
    defaultKeyChain_->sign(*instanceCertificate, signingParams);

    instanceKeyChain_->addCertificate(*instancePibKey, *instanceCertificate);
    instanceKeyChain_->setDefaultCertificate(*instancePibKey, *instanceCertificate);
    instanceKeyChain_->setDefaultIdentity(*instancePibIdentity);
    instanceCert_ = instanceCertificate;

	cout << "Instance certificate "
		<< instanceKeyChain_->getPib().getIdentity(Name(instanceIdentity_))
          ->getDefaultKey()->getDefaultCertificate()->getName() << endl;
}

//void KeyChainManager::registerPrefix(const Name& prefix)
//{
//    shared_ptr<MemoryContentCache> memCache = memoryContentCache_;
//    shared_ptr<ndnlog::new_api::Logger> logger = logger_;
//
//    memoryContentCache_->registerPrefix(prefix,
//        [logger](const shared_ptr<const Name> &p){
//            cout << "Prefix registration failure: " << p->toUri() << endl;
//        },
//        [logger](const shared_ptr<const Name>& p, uint64_t id){
//            cout << "Prefix registration success: " << p->toUri() << endl;
//        },
//        [logger, memCache](const shared_ptr<const Name>& p,
//            const shared_ptr<const Interest> &i,
//            Face& f, uint64_t, const shared_ptr<const InterestFilter>&){
//            cout << "Unexpected interest received " << i->getName()
//            << endl;
//
//            memCache->storePendingInterest(i, f);
//        });
//}

void KeyChainManager::checkExists(const string& file)
{
    ifstream stream(file.c_str());
    bool result = (bool)stream;
    stream.close();

    if (!result)
    {
        stringstream ss;
        ss << "Can't find file " << file << endl;
        throw runtime_error(ss.str());
    }
}
