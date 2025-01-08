/*
 * Copyright (c) 2022 Belledonne Communications SARL.
 *
 * This file is part of postquantumcryptoengine.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/crypto.h"
#include "bctoolbox/crypto.hh"
#include "postquantumcryptoengine/crypto.hh"

#ifdef LIBOQS_USE_BUILD_INTERFACE
#include <oqs.h>
#else
#include <oqs/oqs.h>
#endif

#include <algorithm>

using namespace std;

namespace bctoolbox { // This interface should one day be merged in the bctoolbox project so use bctoolbox namespace

// API doc says we return 0 when no problem, as we forward directly the OQS return code, check it is still true in liboqs
static_assert(OQS_SUCCESS == 0);
// Check our redifinitions of constants (made in the .hh) match the one from liboqs
static_assert(MLKEM512::kSkSize == OQS_KEM_ml_kem_512_length_secret_key, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM512::kPkSize == OQS_KEM_ml_kem_512_length_public_key, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM512::kCtSize == OQS_KEM_ml_kem_512_length_ciphertext, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM512::kSsSize == OQS_KEM_ml_kem_512_length_shared_secret, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM768::kSkSize == OQS_KEM_ml_kem_768_length_secret_key, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM768::kPkSize == OQS_KEM_ml_kem_768_length_public_key, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM768::kCtSize == OQS_KEM_ml_kem_768_length_ciphertext, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM768::kSsSize == OQS_KEM_ml_kem_768_length_shared_secret, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM1024::kSkSize == OQS_KEM_ml_kem_1024_length_secret_key, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM1024::kPkSize == OQS_KEM_ml_kem_1024_length_public_key, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM1024::kCtSize == OQS_KEM_ml_kem_1024_length_ciphertext, "forwarding oqs ml_kem define mismatch");
static_assert(MLKEM1024::kSsSize == OQS_KEM_ml_kem_1024_length_shared_secret, "forwarding oqs ml_kem define mismatch");

static_assert(KYBER512::kSkSize == OQS_KEM_kyber_512_length_secret_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER512::kPkSize == OQS_KEM_kyber_512_length_public_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER512::kCtSize == OQS_KEM_kyber_512_length_ciphertext, "forwarding oqs kyber define mismatch");
static_assert(KYBER512::kSsSize == OQS_KEM_kyber_512_length_shared_secret, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::kSkSize == OQS_KEM_kyber_768_length_secret_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::kPkSize == OQS_KEM_kyber_768_length_public_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::kCtSize == OQS_KEM_kyber_768_length_ciphertext, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::kSsSize == OQS_KEM_kyber_768_length_shared_secret, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::kSkSize == OQS_KEM_kyber_1024_length_secret_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::kPkSize == OQS_KEM_kyber_1024_length_public_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::kCtSize == OQS_KEM_kyber_1024_length_ciphertext, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::kSsSize == OQS_KEM_kyber_1024_length_shared_secret, "forwarding oqs kyber define mismatch");

static_assert(HQC128::kSkSize == OQS_KEM_hqc_128_length_secret_key, "forwarding oqs hqc define mismatch");
static_assert(HQC128::kPkSize == OQS_KEM_hqc_128_length_public_key, "forwarding oqs hqc define mismatch");
static_assert(HQC128::kCtSize == OQS_KEM_hqc_128_length_ciphertext, "forwarding oqs hqc define mismatch");
static_assert(HQC128::kSsSize == OQS_KEM_hqc_128_length_shared_secret, "forwarding oqs hqc define mismatch");
static_assert(HQC192::kSkSize == OQS_KEM_hqc_192_length_secret_key, "forwarding oqs hqc define mismatch");
static_assert(HQC192::kPkSize == OQS_KEM_hqc_192_length_public_key, "forwarding oqs hqc define mismatch");
static_assert(HQC192::kCtSize == OQS_KEM_hqc_192_length_ciphertext, "forwarding oqs hqc define mismatch");
static_assert(HQC192::kSsSize == OQS_KEM_hqc_192_length_shared_secret, "forwarding oqs hqc define mismatch");
static_assert(HQC256::kSkSize == OQS_KEM_hqc_256_length_secret_key, "forwarding oqs hqc define mismatch");
static_assert(HQC256::kPkSize == OQS_KEM_hqc_256_length_public_key, "forwarding oqs hqc define mismatch");
static_assert(HQC256::kCtSize == OQS_KEM_hqc_256_length_ciphertext, "forwarding oqs hqc define mismatch");
static_assert(HQC256::kSsSize == OQS_KEM_hqc_256_length_shared_secret, "forwarding oqs hqc define mismatch");

K25519::K25519(int hashId){
	this->mName = BCTBX_ECDH_X25519;
	this->mId = 0x20;
	this->mHashId = hashId;
}

K448::K448(int hashId){
	this->mName = BCTBX_ECDH_X448;
	this->mId = 0x21;
	this->mHashId = hashId;
}

HYBRID_KEM::HYBRID_KEM(const std::list<std::shared_ptr<KEM>> &algoList, int hashId){
	this->mAlgo = algoList;
	this->mHashId = hashId;
}

/* KEM INTERFACE FOR POST QUANTUM ALGORITHMS */
// Post quantum implementations come from LibOQS

int MLKEM512::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with MLKEM512
	return OQS_KEM_ml_kem_512_keypair(pk.data(), sk.data());
}

int MLKEM512::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with MLKEM512
	return OQS_KEM_ml_kem_512_encaps(ct.data(), ss.data(), pk.data());
}

int MLKEM512::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with MLKEM512
	return OQS_KEM_ml_kem_512_decaps(ss.data(), ct.data(), sk.data());
}

int MLKEM768::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with MLKEM768
	return OQS_KEM_ml_kem_768_keypair(pk.data(), sk.data());
}

int MLKEM768::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with MLKEM768
	return OQS_KEM_ml_kem_768_encaps(ct.data(), ss.data(), pk.data());
}

int MLKEM768::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with MLKEM768
	return OQS_KEM_ml_kem_768_decaps(ss.data(), ct.data(), sk.data());
}

int MLKEM1024::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with MLKEM1024
	return OQS_KEM_ml_kem_1024_keypair(pk.data(), sk.data());
}

int MLKEM1024::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with MLKEM1024
	return OQS_KEM_ml_kem_1024_encaps(ct.data(), ss.data(), pk.data());
}

int MLKEM1024::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with MLKEM1024
	return OQS_KEM_ml_kem_1024_decaps(ss.data(), ct.data(), sk.data());
}

int KYBER512::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with KYBER512
	return OQS_KEM_kyber_512_keypair(pk.data(), sk.data());
}

int KYBER512::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with KYBER512
	return OQS_KEM_kyber_512_encaps(ct.data(), ss.data(), pk.data());
}

int KYBER512::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with KYBER512
	return OQS_KEM_kyber_512_decaps(ss.data(), ct.data(), sk.data());
}

int KYBER768::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with KYBER768
	return OQS_KEM_kyber_768_keypair(pk.data(), sk.data());
}

int KYBER768::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with KYBER768
	return OQS_KEM_kyber_768_encaps(ct.data(), ss.data(), pk.data());
}

int KYBER768::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with KYBER768
	return OQS_KEM_kyber_768_decaps(ss.data(), ct.data(), sk.data());
}

int KYBER1024::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with KYBER1024
	return OQS_KEM_kyber_1024_keypair(pk.data(), sk.data());
}

int KYBER1024::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with KYBER1024
	return OQS_KEM_kyber_1024_encaps(ct.data(), ss.data(), pk.data());
}

int KYBER1024::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with KYBER1024
	return OQS_KEM_kyber_1024_decaps(ss.data(), ct.data(), sk.data());
}


int HQC128::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with HQC128
	return OQS_KEM_hqc_128_keypair(pk.data(), sk.data());
}

int HQC128::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with HQC128
	return OQS_KEM_hqc_128_encaps(ct.data(), ss.data(), pk.data());
}

int HQC128::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with HQC128
	return OQS_KEM_hqc_128_decaps(ss.data(), ct.data(), sk.data());
}

int HQC192::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with HQC192
	return OQS_KEM_hqc_192_keypair(pk.data(), sk.data());
}

int HQC192::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with HQC192
	return OQS_KEM_hqc_192_encaps(ct.data(), ss.data(), pk.data());
}

int HQC192::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with HQC192
	return OQS_KEM_hqc_192_decaps(ss.data(), ct.data(), sk.data());
}

int HQC256::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with HQC256
	return OQS_KEM_hqc_256_keypair(pk.data(), sk.data());
}

int HQC256::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with HQC256
	return OQS_KEM_hqc_256_encaps(ct.data(), ss.data(), pk.data());
}

int HQC256::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with HQC256
	return OQS_KEM_hqc_256_decaps(ss.data(), ct.data(), sk.data());
}

/* KEM INTERFACE FOR CLASSIC ALGORITHMS */

/**
 * @ref     https://datatracker.ietf.org/doc/html/rfc9180
 *
 * @brief   Derivation of the shared secret
 *
 * @param[in/out]   ss          Shared secret
 * @param[in]       ct          Cipher text
 * @param[in]       pk          Public key
 * @param[in]       algoId     Id of the algorithm used to generate the shared secret (https://datatracker.ietf.org/doc/html/rfc9180#section-7.1)
 * @param[in]       secretSize  Shared secret size
 *
 * How are HKDF params built according to the RFC 9180 ?
 *  salt = ""
 *  ikm  = concat("ZRTP", suite_id, label, ikm)
 *      where   suite_id = concat("KEM", I2OSP(kem_id (=algoId), 2))
 *              label    = "eae_prk"
 *              ikm      = ss
 *  info = concat(I2OSP(L (=secretSize), 2), "ZRTP-v1.1", suite_id, label, info)
 *      where   suite_id = concat("KEM", I2OSP(kem_id, 2))
 *              label    = "shared_secret"
 *              info     = kem_context (= concat(ct, pk))
 */
template<typename hashAlgo>
static void secretDerivation(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &pk, const uint8_t algoId, const size_t secretSize){
	// Compute ikm
	string ikmS = "ZRTPKEM";
	vector<uint8_t> ikm{ikmS.cbegin(), ikmS.cend()};
	ikm.push_back((uint8_t)0x00);
	ikm.push_back(algoId);
	ikmS = "eae_prk";
	ikm.insert(ikm.end(), ikmS.cbegin(), ikmS.cend());
	ikm.insert(ikm.end(), ss.cbegin(), ss.cend());
	// Compute kem_context = ct || pk
	vector<uint8_t> kemContext = ct;
	kemContext.insert(kemContext.end(), pk.cbegin(), pk.cend());
	//Compute info
	string infoS = "ZRTP-v1.1KEM";
	vector<uint8_t> info{static_cast<uint8_t>((secretSize>>8)&0xFF), static_cast<uint8_t>((secretSize&0xFF))};
	info.insert(info.end(), infoS.cbegin(), infoS.cend());
	info.push_back((uint8_t)0x00);
	info.push_back(algoId);
	infoS = "shared_secret";
	info.insert(info.end(), infoS.cbegin(), infoS.cend());
	info.insert(info.end(), kemContext.cbegin(), kemContext.cend());

	// Compute new shared secret
	ss = HKDF<hashAlgo>({}, ikm, info, secretSize);
}

template <typename Derived>
int ECDH_KEM<Derived>::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();
	bctbx_ECDHContext_t *alice = bctbx_CreateECDHContext(mName);

	bctbx_ECDHCreateKeyPair(alice, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);

	pk.resize(alice->pointCoordinateLength);
	copy_n(alice->selfPublic, pk.size(), pk.begin());

	sk.resize(alice->secretLength);
	copy_n(alice->secret, sk.size(), sk.begin());

	bctbx_rng_context_free(RNG);
	bctbx_DestroyECDHContext(alice);
	return 0;
}

template <typename Derived>
int ECDH_KEM<Derived>::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();
	bctbx_ECDHContext_t *bob = bctbx_CreateECDHContext(mName);

	if(pk.size() != bob->pointCoordinateLength) {
		bctbx_rng_context_free(RNG);
		bctbx_DestroyECDHContext(bob);
		return 1;
	}

	ct.resize(bob->pointCoordinateLength);

	bctbx_ECDHSetPeerPublicKey(bob, pk.data(), pk.size());

	/* Calculate Bob's public key and shared secret */
	bctbx_ECDHCreateKeyPair(bob, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);
	bctbx_ECDHComputeSecret(bob, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);

	copy_n(bob->selfPublic, ct.size(), ct.begin());

	ss.resize(bob->secretLength);
	copy_n(bob->sharedSecret, bob->secretLength, ss.begin());

	// Derivation of the shared secret (ss)
	switch(mHashId){
		case BCTBX_MD_SHA256: secretDerivation<SHA256>(ss, ct, pk, mId, bob->pointCoordinateLength); break;
		case BCTBX_MD_SHA384: secretDerivation<SHA384>(ss, ct, pk, mId, bob->pointCoordinateLength); break;
		case BCTBX_MD_SHA512: secretDerivation<SHA512>(ss, ct, pk, mId, bob->pointCoordinateLength); break;
	}

	bctbx_rng_context_free(RNG);
	bctbx_DestroyECDHContext(bob);
	return 0;
}

template <typename Derived>
int ECDH_KEM<Derived>::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();
	bctbx_ECDHContext_t *alice = bctbx_CreateECDHContext(mName);

	if(sk.size() != alice->secretLength || ct.size() != alice->pointCoordinateLength) {
		bctbx_rng_context_free(RNG);
		bctbx_DestroyECDHContext(alice);
		return 1;
	}
	bctbx_ECDHSetSecretKey(alice, sk.data(), sk.size());

	bctbx_ECDHSetPeerPublicKey(alice, ct.data(), ct.size());
	bctbx_ECDHComputeSecret(alice, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);

	ss.resize(alice->pointCoordinateLength);
	copy_n(alice->sharedSecret, alice->pointCoordinateLength, ss.begin());

	// Recovery of pk
	bctbx_ECDHDerivePublicKey(alice);
	vector<uint8_t> pk{alice->selfPublic, alice->selfPublic+alice->pointCoordinateLength};

	// Derivation of the shared secret (ss)
	switch(mHashId){
		case BCTBX_MD_SHA256: secretDerivation<SHA256>(ss, ct, pk, mId, alice->pointCoordinateLength); break;
		case BCTBX_MD_SHA384: secretDerivation<SHA384>(ss, ct, pk, mId, alice->pointCoordinateLength); break;
		case BCTBX_MD_SHA512: secretDerivation<SHA512>(ss, ct, pk, mId, alice->pointCoordinateLength); break;
	}

	bctbx_rng_context_free(RNG);
	bctbx_DestroyECDHContext(alice);
	return 0;
}

/* COMBINER ENCAPSULATION AND DECAPSULATION FUNCTIONS */

namespace {
/**
 * @brief Create (public/private) key pairs
 *
 * @param[in]   algoList   List of exchanging key algorithms
 * @param[out]  pkList     List of public keys
 * @param[out]  skList     List of secret keys
 *
 * @return  0 if no problem
 */
int genKeypairs(const list<shared_ptr<KEM>> algoList, vector<vector<uint8_t>> &pkList, vector<vector<uint8_t>> &skList){
	pkList.reserve(algoList.size());
	skList.reserve(algoList.size());
	int ret = 0;
	for (const auto &a : algoList) {
		vector<uint8_t> pk, sk;
		ret += a->keyGen(pk, sk);
		pkList.push_back(std::move(pk));
		skList.push_back(std::move(sk));
	}
	return ret;
}

template<typename hashAlgo>
vector<uint8_t> nestedDualPrf(vector<vector<uint8_t>> &list){
	vector<uint8_t> T;
	for(auto &e : list){
		T = HMAC<hashAlgo>(T, e);
		// The list given to this function is all secret but the last one
		// clean them when we don't need them anymore
		if (&e != &list.back()) {
			bctbx_clean(e.data(), e.size());
		}
	}
	return T;
}

/**
 * @brief Encapsulation N combiner
 *
 * @param[in]   algoList   List of exchanging key algorithms
 * @param[out]  ss          Hybrid shared secret
 * @param[out]  ctList     List of cipher texts
 * @param[in]   pkList     List of public keys
 *
 * @return  code error (0 if no problem)
 */
int NCombinerEncaps(const list<shared_ptr<KEM>> &algoList, int hashId, vector<uint8_t> &ss, vector<vector<uint8_t>> &ctList, const vector<vector<uint8_t>> &pkList){
	vector<vector<uint8_t>> secretList;

	// Compute cipher text and secret for each algo
	int i=0;
	for(const auto &e : algoList){
		vector<uint8_t> secret, ct;
		e->encaps(ct, secret, pkList.at(i));
		secretList.push_back(std::move(secret));
		ctList.push_back(std::move(ct));
		i++;
	}

	// Concatenation of the cipher texts in ct
	vector<uint8_t> allCt;
	for(const auto &e : ctList){
		allCt.insert(allCt.end(), e.cbegin(), e.cend());
	}

	// Compute the shared secret using a PRF
	secretList.push_back(std::move(allCt));
	switch(hashId){
		case BCTBX_MD_SHA256: ss = nestedDualPrf<SHA256>(secretList); break;
		case BCTBX_MD_SHA384: ss = nestedDualPrf<SHA384>(secretList); break;
		case BCTBX_MD_SHA512: ss = nestedDualPrf<SHA512>(secretList); break;
	}

	return 0;
}

/**
 * @brief Decapsulation N combiner
 *
 * @param[in]   algoList   List of exchanging key algorithms
 * @param[out]  ss          Hybrid shared secret
 * @param[in]   ctList     List of cipher texts
 * @param[in]   skList     List of secret keys
 *
 * @return  code error (0 if no problem)
 */
int NCombinerDecaps(const list<shared_ptr<KEM>> &algoList, int hashId, vector<uint8_t> &ss, const vector<vector<uint8_t>> &ctList, const vector<vector<uint8_t>> &skList){
	vector<vector<uint8_t>> secretList;

	// Compute secret for each algo
	int i = 0;
	for(const auto &e : algoList){
		vector<uint8_t> secret;
		e->decaps(secret, ctList.at(i), skList.at(i));
		secretList.push_back(std::move(secret));
		i++;
	}

	// Concatenation of the both cipher texts in ct
	vector<uint8_t> ct;
	for(const auto &e : ctList){
		ct.insert(ct.end(), e.cbegin(), e.cend());
	}

	// Compute the shared secret using a PRF
	secretList.push_back(ct);
	switch(hashId){
		case BCTBX_MD_SHA256: ss = nestedDualPrf<SHA256>(secretList); break;
		case BCTBX_MD_SHA384: ss = nestedDualPrf<SHA384>(secretList); break;
		case BCTBX_MD_SHA512: ss = nestedDualPrf<SHA512>(secretList); break;
	}
	return 0;
}
}// anonymous namespace

/* HYBRID KEM INTERFACE */
size_t HYBRID_KEM::getSkSize() const noexcept {
	size_t ret = 0;
	for (const auto &e : mAlgo) {
		ret += e->getSkSize();
	}
	return ret;
}

size_t HYBRID_KEM::getPkSize() const noexcept {
	size_t ret = 0;
	for (const auto &e : mAlgo) {
		ret += e->getPkSize();
	}
	return ret;
}

size_t HYBRID_KEM::getCtSize() const noexcept {
	size_t ret = 0;
	for (const auto &e : mAlgo) {
		ret += e->getCtSize();
	}
	return ret;
}

size_t HYBRID_KEM::getSsSize() const noexcept {
	size_t ret = 0;
	for (const auto &e : mAlgo) {
		ret += e->getSsSize();
	}
	return ret;
}

// Use the combiner functions described before
int HYBRID_KEM::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	vector<vector<uint8_t>> pkList;
	vector<vector<uint8_t>> skList;

	int ret = genKeypairs(mAlgo, pkList, skList);

	pk.reserve(getPkSize());
	for (const auto &e : pkList) {
		pk.insert(pk.end(), e.cbegin(), e.cend());
	}

	sk.reserve(getSkSize());
	for (auto &e : skList) {
		sk.insert(sk.end(), e.cbegin(), e.cend());
		bctbx_clean(e.data(), e.size());
	}

	return ret;
}

int HYBRID_KEM::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	vector<vector<uint8_t>> ctList;
	vector<vector<uint8_t>> pkList;

	ctList.reserve(mAlgo.size());
	pkList.reserve(mAlgo.size());

	size_t ctSize = 0;
	auto iter = pk.cbegin();
	for (const auto &e : mAlgo) {
		auto pkSize = e->getPkSize();
		pkList.emplace_back(iter, iter+pkSize);
		std::advance(iter, pkSize);
		ctSize += e->getCtSize(); // compute the final ct size to reserve space in output ct
	}

	int ret = NCombinerEncaps(mAlgo, mHashId, ss, ctList, pkList);

	ct.reserve(ctSize);
	for (const auto &e : ctList) {
		ct.insert(ct.end(), e.cbegin(), e.cend());
	}
	return ret;
}

int HYBRID_KEM::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	vector<vector<uint8_t>> ctList;
	vector<vector<uint8_t>> skList;

	ctList.reserve(mAlgo.size());
	skList.reserve(mAlgo.size());

	auto iter = sk.cbegin();
	auto iter2 = ct.cbegin();
	for (const auto &e : mAlgo) {
		auto skSize = e->getSkSize();
		auto ctSize = e->getCtSize();
		skList.emplace_back(iter, iter + skSize);
		std::advance(iter, skSize);
		ctList.emplace_back(iter2, iter2 + ctSize);
		std::advance(iter2, ctSize);
	}

	return NCombinerDecaps(mAlgo, mHashId, ss, ctList, skList);
}

// force templates intanciation
template class ECDH_KEM<K25519>;
template class ECDH_KEM<K448>;

} /* namespace bctoolbox */


