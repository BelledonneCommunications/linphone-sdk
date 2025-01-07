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

/* Check our redifinitions of constants (made in the .hh) match the one from liboqs */
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


template <typename Derived> size_t KEMCRTP<Derived>::getSkSize() const {
	if (Derived::kSkSize > 0) {
		return Derived::kSkSize;
	} else {
		throw BCTBX_EXCEPTION<<"invalid sk size retrieved from derived class";
	};
}

template <typename Derived> size_t KEMCRTP<Derived>::getPkSize() const {
	if (Derived::kPkSize > 0) {
		return Derived::kPkSize;
	} else {
		throw BCTBX_EXCEPTION<<"invalid pk size retrieved from derived class";
	};
}

template <typename Derived> size_t KEMCRTP<Derived>::getCtSize() const {
	if (Derived::kCtSize > 0) {
		return Derived::kCtSize;
	} else {
		throw BCTBX_EXCEPTION<<"invalid ct size retrieved from derived class";
	};
}

template <typename Derived> size_t KEMCRTP<Derived>::getSsSize() const {
	if (Derived::kSsSize > 0) {
		return Derived::kSsSize;
	} else {
		throw BCTBX_EXCEPTION<<"invalid ss size retrieved from derived class";
	};
}

K25519::K25519(int hash_id){
	this->mName = BCTBX_ECDH_X25519;
	this->mId = 0x20;
	this->mHashId = hash_id;
}

K448::K448(int hash_id){
	this->mName = BCTBX_ECDH_X448;
	this->mId = 0x21;
	this->mHashId = hash_id;
}

HYBRID_KEM::HYBRID_KEM(const std::list<std::shared_ptr<KEM>> &algoList, int hash_id){
	this->mAlgo = algoList;
	this->mHashId = hash_id;
}

/* KEM INTERFACE FOR POST QUANTUM ALGORITHMS */
// Post quantum implementations come from LibOQS

int MLKEM512::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with MLKEM512
	int res = OQS_KEM_ml_kem_512_keypair(pk.data(), sk.data());

	return res;
}

int MLKEM512::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with MLKEM512
	int res = OQS_KEM_ml_kem_512_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int MLKEM512::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with MLKEM512
	int res = OQS_KEM_ml_kem_512_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int MLKEM768::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with MLKEM768
	int res = OQS_KEM_ml_kem_768_keypair(pk.data(), sk.data());

	return res;
}

int MLKEM768::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with MLKEM768
	int res = OQS_KEM_ml_kem_768_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int MLKEM768::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with MLKEM768
	int res = OQS_KEM_ml_kem_768_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int MLKEM1024::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with MLKEM1024
	int res = OQS_KEM_ml_kem_1024_keypair(pk.data(), sk.data());

	return res;
}

int MLKEM1024::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with MLKEM1024
	int res = OQS_KEM_ml_kem_1024_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int MLKEM1024::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with MLKEM1024
	int res = OQS_KEM_ml_kem_1024_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int KYBER512::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with KYBER512
	int res = OQS_KEM_kyber_512_keypair(pk.data(), sk.data());

	return res;
}

int KYBER512::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with KYBER512
	int res = OQS_KEM_kyber_512_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int KYBER512::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with KYBER512
	int res = OQS_KEM_kyber_512_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int KYBER768::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with KYBER768
	int res = OQS_KEM_kyber_768_keypair(pk.data(), sk.data());

	return res;
}

int KYBER768::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with KYBER768
	int res = OQS_KEM_kyber_768_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int KYBER768::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with KYBER768
	int res = OQS_KEM_kyber_768_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int KYBER1024::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with KYBER1024
	int res = OQS_KEM_kyber_1024_keypair(pk.data(), sk.data());

	return res;
}

int KYBER1024::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with KYBER1024
	int res = OQS_KEM_kyber_1024_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int KYBER1024::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with KYBER1024
	int res = OQS_KEM_kyber_1024_decaps(ss.data(), ct.data(), sk.data());

	return res;
}


int HQC128::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with HQC128
	int res = OQS_KEM_hqc_128_keypair(pk.data(), sk.data());

	return res;
}

int HQC128::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with HQC128
	int res = OQS_KEM_hqc_128_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int HQC128::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with HQC128
	int res = OQS_KEM_hqc_128_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int HQC192::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with HQC192
	int res = OQS_KEM_hqc_192_keypair(pk.data(), sk.data());

	return res;
}

int HQC192::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with HQC192
	int res = OQS_KEM_hqc_192_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int HQC192::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with HQC192
	int res = OQS_KEM_hqc_192_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int HQC256::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(kPkSize);
	sk.resize(kSkSize);

	// Key pair generation function with HQC256
	int res = OQS_KEM_hqc_256_keypair(pk.data(), sk.data());

	return res;
}

int HQC256::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(kCtSize);
	ss.resize(kSsSize);

	// Key encapsulation with HQC256
	int res = OQS_KEM_hqc_256_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int HQC256::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(kSsSize);

	// Key decapsulation with HQC256
	int res = OQS_KEM_hqc_256_decaps(ss.data(), ct.data(), sk.data());

	return res;
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
 * @param[in]       algo_id     Id of the algorithm used to generate the shared secret (https://datatracker.ietf.org/doc/html/rfc9180#section-7.1)
 * @param[in]       secretSize  Shared secret size
 *
 * How are HKDF params built according to the RFC 9180 ?
 *  salt = ""
 *  ikm  = concat("ZRTP", suite_id, label, ikm)
 *      where   suite_id = concat("KEM", I2OSP(kem_id (=algo_id), 2))
 *              label    = "eae_prk"
 *              ikm      = ss
 *  info = concat(I2OSP(L (=secretSize), 2), "ZRTP-v1.1", suite_id, label, info)
 *      where   suite_id = concat("KEM", I2OSP(kem_id, 2))
 *              label    = "shared_secret"
 *              info     = kem_context (= concat(ct, pk))
 */
template<typename hashAlgo>
static void secret_derivation(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &pk, const uint8_t algo_id, const size_t secretSize){
	// Compute ikm
	string ikm_s = "ZRTPKEM";
	vector<uint8_t> ikm{ikm_s.begin(), ikm_s.end()};
	ikm.push_back((uint8_t)0x00);
	ikm.push_back(algo_id);
	ikm_s = "eae_prk";
	ikm.insert(ikm.end(), ikm_s.begin(), ikm_s.end());
	ikm.insert(ikm.end(), ss.begin(), ss.end());
	// Compute kem_context
	vector<uint8_t> kem_context{ct.begin(), ct.end()};
	kem_context.insert(kem_context.end(), pk.begin(), pk.end());
	//Compute info
	string info_s = "ZRTP-v1.1KEM";
	vector<uint8_t> info{static_cast<uint8_t>((secretSize>>8)&0xFF), static_cast<uint8_t>((secretSize&0xFF))};
	info.insert(info.end(), info_s.begin(), info_s.end());
	info.push_back((uint8_t)0x00);
	info.push_back(algo_id);
	info_s = "shared_secret";
	info.insert(info.end(), info_s.begin(), info_s.end());
	info.insert(info.end(), kem_context.begin(), kem_context.end());

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
		case BCTBX_MD_SHA256: secret_derivation<SHA256>(ss, ct, pk, mId, bob->pointCoordinateLength); break;
		case BCTBX_MD_SHA384: secret_derivation<SHA384>(ss, ct, pk, mId, bob->pointCoordinateLength); break;
		case BCTBX_MD_SHA512: secret_derivation<SHA512>(ss, ct, pk, mId, bob->pointCoordinateLength); break;
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
		case BCTBX_MD_SHA256: secret_derivation<SHA256>(ss, ct, pk, mId, alice->pointCoordinateLength); break;
		case BCTBX_MD_SHA384: secret_derivation<SHA384>(ss, ct, pk, mId, alice->pointCoordinateLength); break;
		case BCTBX_MD_SHA512: secret_derivation<SHA512>(ss, ct, pk, mId, alice->pointCoordinateLength); break;
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
 * @param[in]   algo_list   List of exchanging key algorithms
 * @param[out]  pk_list     List of public keys
 * @param[out]  sk_list     List of secret keys
 *
 * @return  0 if no problem
 */
int crypto_gen_keypairs(const list<shared_ptr<KEM>> algo_list, vector<vector<uint8_t>> &pk_list, vector<vector<uint8_t>> &sk_list){
	pk_list.reserve(algo_list.size());
	sk_list.reserve(algo_list.size());
	for (const auto &a : algo_list) {
		vector<uint8_t> pk, sk;
		a->keyGen(pk, sk);
		pk_list.push_back(std::move(pk));
		sk_list.push_back(std::move(sk));
	}
	return 0;
}

template<typename hashAlgo>
vector<uint8_t> nested_dual_PRF(vector<vector<uint8_t>> &list){
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
 * @param[in]   algo_list   List of exchanging key algorithms
 * @param[out]  ss          Hybrid shared secret
 * @param[out]  ct_list     List of cipher texts
 * @param[in]   pk_list     List of public keys
 *
 * @return  code error (0 if no problem)
 */
int crypto_enc_N(const list<shared_ptr<KEM>> &algo_list, int hash_id, vector<uint8_t> &ss, vector<vector<uint8_t>> &ct_list, const vector<vector<uint8_t>> &pk_list){
	vector<vector<uint8_t>> secret_list;

	// Compute cipher text and secret for each algo
	int i=0;
	for(const auto &e : algo_list){
		vector<uint8_t> secret, ct;
		e->encaps(ct, secret, pk_list.at(i));
		secret_list.push_back(std::move(secret));
		ct_list.push_back(std::move(ct));
		i++;
	}

	// Concatenation of the cipher texts in ct
	vector<uint8_t> allCt;
	for(const auto &e : ct_list){
		allCt.insert(allCt.end(), e.cbegin(), e.cend());
	}

	// Compute the shared secret using a PRF
	secret_list.push_back(std::move(allCt));
	switch(hash_id){
		case BCTBX_MD_SHA256: ss = nested_dual_PRF<SHA256>(secret_list); break;
		case BCTBX_MD_SHA384: ss = nested_dual_PRF<SHA384>(secret_list); break;
		case BCTBX_MD_SHA512: ss = nested_dual_PRF<SHA512>(secret_list); break;
	}

	return 0;
}

/**
 * @brief Decapsulation N combiner
 *
 * @param[in]   algo_list   List of exchanging key algorithms
 * @param[out]  ss          Hybrid shared secret
 * @param[in]   ct_list     List of cipher texts
 * @param[in]   sk_list     List of secret keys
 *
 * @return  code error (0 if no problem)
 */
int crypto_dec_N(const list<shared_ptr<KEM>> &algo_list, int hash_id, vector<uint8_t> &ss, const vector<vector<uint8_t>> &ct_list, const vector<vector<uint8_t>> &sk_list){
	vector<vector<uint8_t>> secret_list;

	// Compute secret for each algo
	int i = 0;
	for(const auto &e : algo_list){
		vector<uint8_t> secret;
		e->decaps(secret, ct_list.at(i), sk_list.at(i));
		secret_list.push_back(std::move(secret));
		i++;
	}

	// Concatenation of the both cipher texts in ct
	vector<uint8_t> ct;
	for(const auto &e : ct_list){
		ct.insert(ct.end(), e.cbegin(), e.cend());
	}

	// Compute the shared secret using a PRF
	secret_list.push_back(ct);
	switch(hash_id){
		case BCTBX_MD_SHA256: ss = nested_dual_PRF<SHA256>(secret_list); break;
		case BCTBX_MD_SHA384: ss = nested_dual_PRF<SHA384>(secret_list); break;
		case BCTBX_MD_SHA512: ss = nested_dual_PRF<SHA512>(secret_list); break;
	}
	return 0;
}
}// anonymous namespace

/* HYBRID KEM INTERFACE */
// Use the combiner functions described before

int HYBRID_KEM::keyGen(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	vector<vector<uint8_t>> pkList;
	vector<vector<uint8_t>> skList;

	int ret = crypto_gen_keypairs(mAlgo, pkList, skList);

	for (const auto &e : pkList) {
		pk.insert(pk.end(), e.cbegin(), e.cend());
	}

	for (auto &e : skList) {
		sk.insert(sk.end(), e.cbegin(), e.cend());
		bctbx_clean(e.data(), e.size());
	}

	return ret;
}

int HYBRID_KEM::encaps(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	vector<vector<uint8_t>> ctList;
	vector<vector<uint8_t>> pkList;

	ctList.resize(mAlgo.size());
	pkList.resize(mAlgo.size());

	auto iter = pk.cbegin();
	int i = 0;
	for (const auto &e : mAlgo) {
		pkList.at(i).insert(pkList.at(i).end(), iter, iter+e->getPkSize());
		iter += e->getPkSize();
		i++;
	}

	int ret = crypto_enc_N(mAlgo, mHashId, ss, ctList, pkList);

	for (const auto &e : ctList) {
		ct.insert(ct.end(), e.cbegin(), e.cend());
	}
	return ret;
}

int HYBRID_KEM::decaps(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	vector<vector<uint8_t>> ctList;
	vector<vector<uint8_t>> skList;

	ctList.resize(mAlgo.size());
	skList.resize(mAlgo.size());

	auto iter = sk.cbegin();
	auto iter2 = ct.cbegin();
	int i = 0;
	for (const auto &e : mAlgo) {
		skList.at(i).insert(skList.at(i).end(), iter, iter+e->getSkSize());
		iter += e->getSkSize();
		ctList.at(i).insert(ctList.at(i).end(), iter2, iter2+e->getCtSize());
		iter2 += e->getCtSize();
		i++;
	}

	return crypto_dec_N(mAlgo, mHashId, ss, ctList, skList);
}

// force templates intanciation
template class ECDH_KEM<K25519>;
template class ECDH_KEM<K448>;

} /* namespace bctoolbox */


