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

#include <oqs/oqs.h>

#include <algorithm>

using namespace std;

namespace bctoolbox { // This interface should one day be merged in the bctoolbox project so use bctoolbox namespace

/* Check our redifinitions of constants (made in the .hh) match the one from liboqs */
static_assert(KYBER512::skSize == OQS_KEM_kyber_512_length_secret_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER512::pkSize == OQS_KEM_kyber_512_length_public_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER512::ctSize == OQS_KEM_kyber_512_length_ciphertext, "forwarding oqs kyber define mismatch");
static_assert(KYBER512::ssSize == OQS_KEM_kyber_512_length_shared_secret, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::skSize == OQS_KEM_kyber_768_length_secret_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::pkSize == OQS_KEM_kyber_768_length_public_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::ctSize == OQS_KEM_kyber_768_length_ciphertext, "forwarding oqs kyber define mismatch");
static_assert(KYBER768::ssSize == OQS_KEM_kyber_768_length_shared_secret, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::skSize == OQS_KEM_kyber_1024_length_secret_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::pkSize == OQS_KEM_kyber_1024_length_public_key, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::ctSize == OQS_KEM_kyber_1024_length_ciphertext, "forwarding oqs kyber define mismatch");
static_assert(KYBER1024::ssSize == OQS_KEM_kyber_1024_length_shared_secret, "forwarding oqs kyber define mismatch");

static_assert(SIKE434::skSize == OQS_KEM_sike_p434_length_secret_key, "forwarding oqs sike define mismatch");
static_assert(SIKE434::pkSize == OQS_KEM_sike_p434_length_public_key, "forwarding oqs sike define mismatch");
static_assert(SIKE434::ctSize == OQS_KEM_sike_p434_length_ciphertext, "forwarding oqs sike define mismatch");
static_assert(SIKE434::ssSize == OQS_KEM_sike_p434_length_shared_secret, "forwarding oqs sike define mismatch");
static_assert(SIKE610::skSize == OQS_KEM_sike_p610_length_secret_key, "forwarding oqs sike define mismatch");
static_assert(SIKE610::pkSize == OQS_KEM_sike_p610_length_public_key, "forwarding oqs sike define mismatch");
static_assert(SIKE610::ctSize == OQS_KEM_sike_p610_length_ciphertext, "forwarding oqs sike define mismatch");
static_assert(SIKE610::ssSize == OQS_KEM_sike_p610_length_shared_secret, "forwarding oqs sike define mismatch");
static_assert(SIKE751::skSize == OQS_KEM_sike_p751_length_secret_key, "forwarding oqs sike define mismatch");
static_assert(SIKE751::pkSize == OQS_KEM_sike_p751_length_public_key, "forwarding oqs sike define mismatch");
static_assert(SIKE751::ctSize == OQS_KEM_sike_p751_length_ciphertext, "forwarding oqs sike define mismatch");
static_assert(SIKE751::ssSize == OQS_KEM_sike_p751_length_shared_secret, "forwarding oqs sike define mismatch");

K25519::K25519(int hash_id){
	this->name = BCTBX_ECDH_X25519;
	this->id = 0x20;
	this->hash_id = hash_id;
}

K448::K448(int hash_id){
	this->name = BCTBX_ECDH_X448;
	this->id = 0x21;
	this->hash_id = hash_id;
}

HYBRID_KEM::HYBRID_KEM(const std::list<std::shared_ptr<KEM>> &algoList, int hash_id){
	this->algo = algoList;
	this->hash_id = hash_id;
}

size_t K25519::get_skSize() const noexcept {
	return skSize;
}
size_t K25519::get_pkSize() const noexcept {
	return pkSize;
}
size_t K25519::get_ctSize() const noexcept {
	return ctSize;
}
size_t K25519::get_ssSize() const noexcept {
	return ssSize;
}

size_t K448::get_skSize() const noexcept {
	return skSize;
}
size_t K448::get_pkSize() const noexcept {
	return pkSize;
}
size_t K448::get_ctSize() const noexcept {
	return ctSize;
}
size_t K448::get_ssSize() const noexcept {
	return ssSize;
}

size_t KYBER512::get_skSize() const noexcept {
	return skSize;
}
size_t KYBER512::get_pkSize() const noexcept {
	return pkSize;
}
size_t KYBER512::get_ctSize() const noexcept {
	return ctSize;
}
size_t KYBER512::get_ssSize() const noexcept {
	return ssSize;
}

size_t KYBER768::get_skSize() const noexcept {
	return skSize;
}
size_t KYBER768::get_pkSize() const noexcept {
	return pkSize;
}
size_t KYBER768::get_ctSize() const noexcept {
	return ctSize;
}
size_t KYBER768::get_ssSize() const noexcept {
	return ssSize;
}

size_t KYBER1024::get_skSize() const noexcept {
	return skSize;
}
size_t KYBER1024::get_pkSize() const noexcept {
	return pkSize;
}
size_t KYBER1024::get_ctSize() const noexcept {
	return ctSize;
}
size_t KYBER1024::get_ssSize() const noexcept {
	return ssSize;
}

size_t SIKE434::get_skSize() const noexcept {
	return skSize;
}
size_t SIKE434::get_pkSize() const noexcept {
	return pkSize;
}
size_t SIKE434::get_ctSize() const noexcept {
	return ctSize;
}
size_t SIKE434::get_ssSize() const noexcept {
	return ssSize;
}

size_t SIKE610::get_skSize() const noexcept {
	return skSize;
}
size_t SIKE610::get_pkSize() const noexcept {
	return pkSize;
}
size_t SIKE610::get_ctSize() const noexcept {
	return ctSize;
}
size_t SIKE610::get_ssSize() const noexcept {
	return ssSize;
}

size_t SIKE751::get_skSize() const noexcept {
	return skSize;
}
size_t SIKE751::get_pkSize() const noexcept {
	return pkSize;
}
size_t SIKE751::get_ctSize() const noexcept {
	return ctSize;
}
size_t SIKE751::get_ssSize() const noexcept {
	return ssSize;
}

size_t HYBRID_KEM::get_skSize() const noexcept {
	size_t size = 0;
	for(shared_ptr<KEM> e : algo){
		size += e->get_skSize();
	}
	return size;
}
size_t HYBRID_KEM::get_pkSize() const noexcept {
	size_t size = 0;
	for(shared_ptr<KEM> e : algo){
		size += e->get_pkSize();
	}
	return size;
}
size_t HYBRID_KEM::get_ctSize() const noexcept {
	size_t size = 0;
	for(shared_ptr<KEM> e : algo){
		size += e->get_ctSize();
	}
	return size;
}
size_t HYBRID_KEM::get_ssSize() const noexcept {
	switch(hash_id){
		case BCTBX_MD_SHA256: return SHA256::ssize();
		case BCTBX_MD_SHA384: return SHA384::ssize();
		case BCTBX_MD_SHA512: return SHA512::ssize();
		default: return 0;
	}
}

/* KEM INTERFACE FOR POST QUANTUM ALGORITHMS */
// Post quantum implementations come from LibOQS

int KYBER512::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(pkSize);
	sk.resize(skSize);

	// Key pair generation function with KYBER512
	int res = OQS_KEM_kyber_512_keypair(pk.data(), sk.data());

	return res;
}

int KYBER512::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(ctSize);
	ss.resize(ssSize);

	// Key encapsulation with KYBER512
	int res = OQS_KEM_kyber_512_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int KYBER512::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(ssSize);

	// Key decapsulation with KYBER512
	int res = OQS_KEM_kyber_512_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int KYBER768::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(pkSize);
	sk.resize(skSize);

	// Key pair generation function with KYBER768
	int res = OQS_KEM_kyber_768_keypair(pk.data(), sk.data());

	return res;
}

int KYBER768::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(ctSize);
	ss.resize(ssSize);

	// Key encapsulation with KYBER768
	int res = OQS_KEM_kyber_768_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int KYBER768::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(ssSize);

	// Key decapsulation with KYBER768
	int res = OQS_KEM_kyber_768_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int KYBER1024::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(pkSize);
	sk.resize(skSize);

	// Key pair generation function with KYBER1024
	int res = OQS_KEM_kyber_1024_keypair(pk.data(), sk.data());

	return res;
}

int KYBER1024::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(ctSize);
	ss.resize(ssSize);

	// Key encapsulation with KYBER1024
	int res = OQS_KEM_kyber_1024_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int KYBER1024::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(ssSize);

	// Key decapsulation with KYBER1024
	int res = OQS_KEM_kyber_1024_decaps(ss.data(), ct.data(), sk.data());

	return res;
}


int SIKE434::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(pkSize);
	sk.resize(skSize);

	// Key pair generation function with SIKE434
	int res = OQS_KEM_sike_p434_keypair(pk.data(), sk.data());

	return res;
}

int SIKE434::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(ctSize);
	ss.resize(ssSize);

	// Key encapsulation with SIKE434
	int res = OQS_KEM_sike_p434_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int SIKE434::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(ssSize);

	// Key decapsulation with SIKE434
	int res = OQS_KEM_sike_p434_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int SIKE610::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(pkSize);
	sk.resize(skSize);

	// Key pair generation function with SIKE610
	int res = OQS_KEM_sike_p610_keypair(pk.data(), sk.data());

	return res;
}

int SIKE610::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(ctSize);
	ss.resize(ssSize);

	// Key encapsulation with SIKE610
	int res = OQS_KEM_sike_p610_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int SIKE610::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(ssSize);

	// Key decapsulation with SIKE610
	int res = OQS_KEM_sike_p610_decaps(ss.data(), ct.data(), sk.data());

	return res;
}

int SIKE751::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	pk.resize(pkSize);
	sk.resize(skSize);

	// Key pair generation function with SIKE751
	int res = OQS_KEM_sike_p751_keypair(pk.data(), sk.data());

	return res;
}

int SIKE751::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	ct.resize(ctSize);
	ss.resize(ssSize);

	// Key encapsulation with SIKE751
	int res = OQS_KEM_sike_p751_encaps(ct.data(), ss.data(), pk.data());

	return res;
}

int SIKE751::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	ss.resize(ssSize);

	// Key decapsulation with SIKE751
	int res = OQS_KEM_sike_p751_decaps(ss.data(), ct.data(), sk.data());

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
 * @param[in]		hash_id		Id of the hash functions used in the HKDF
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

int ECDH_KEM::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();
	bctbx_ECDHContext_t *alice = bctbx_CreateECDHContext(name);

	bctbx_ECDHCreateKeyPair(alice, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);

	pk.resize(get_pkSize());
	copy_n(alice->selfPublic, get_pkSize(), pk.begin());

	sk.resize(get_skSize());
	copy_n(alice->secret, get_skSize(), sk.begin());

	bctbx_rng_context_free(RNG);
	bctbx_DestroyECDHContext(alice);
	return 0;
}

int ECDH_KEM::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();
	bctbx_ECDHContext_t *bob = bctbx_CreateECDHContext(name);

	ct.resize(get_ctSize());

	bctbx_ECDHSetPeerPublicKey(bob, pk.data(), get_pkSize());

	/* Calculate Bob's public key and shared secret */
	bctbx_ECDHCreateKeyPair(bob, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);
	bctbx_ECDHComputeSecret(bob, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);

	copy_n(bob->selfPublic, get_ctSize(), ct.begin());

	ss.resize(bob->secretLength);
	copy_n(bob->sharedSecret, bob->secretLength, ss.begin());

	// Derivation of the shared secret (ss)
	switch(hash_id){
		case BCTBX_MD_SHA256: secret_derivation<SHA256>(ss, ct, pk, id, bob->pointCoordinateLength); break;
		case BCTBX_MD_SHA384: secret_derivation<SHA384>(ss, ct, pk, id, bob->pointCoordinateLength); break;
		case BCTBX_MD_SHA512: secret_derivation<SHA512>(ss, ct, pk, id, bob->pointCoordinateLength); break;
	}

	bctbx_rng_context_free(RNG);
	bctbx_DestroyECDHContext(bob);
	return 0;
}

int ECDH_KEM::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	bctbx_rng_context_t *RNG = bctbx_rng_context_new();
	bctbx_ECDHContext_t *alice = bctbx_CreateECDHContext(name);

	if(sk.size() != get_skSize()) return 1;
	bctbx_ECDHSetSecretKey(alice, sk.data(), sk.size());

	bctbx_ECDHSetPeerPublicKey(alice, ct.data(), ct.size());
	bctbx_ECDHComputeSecret(alice, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, RNG);

	ss.resize(alice->pointCoordinateLength);
	copy_n(alice->sharedSecret, alice->pointCoordinateLength, ss.begin());

	// Recovery of pk
	bctbx_ECDHDerivePublicKey(alice);
	vector<uint8_t> pk{alice->selfPublic, alice->selfPublic+alice->pointCoordinateLength};

	// Derivation of the shared secret (ss)
	switch(hash_id){
		case BCTBX_MD_SHA256: secret_derivation<SHA256>(ss, ct, pk, id, alice->pointCoordinateLength); break;
		case BCTBX_MD_SHA384: secret_derivation<SHA384>(ss, ct, pk, id, alice->pointCoordinateLength); break;
		case BCTBX_MD_SHA512: secret_derivation<SHA512>(ss, ct, pk, id, alice->pointCoordinateLength); break;
	}

	bctbx_rng_context_free(RNG);
	bctbx_DestroyECDHContext(alice);
	return 0;
}

/* COMBINER ENCAPSULATION AND DECAPSULATION FUNCTIONS */

/**
 * @brief Create (public/private) key pairs
 *
 * @param[in]   algo_list   List of exchanging key algorithms
 * @param[out]  pk_list     List of public keys
 * @param[out]  sk_list     List of secret keys
 *
 * @return  0 if no problem
 */
static int crypto_gen_keypairs(const list<shared_ptr<KEM>> algo_list, vector<vector<uint8_t>> &pk_list, vector<vector<uint8_t>> &sk_list){
	vector<uint8_t> pk, sk;
	for (shared_ptr<KEM> a : algo_list) {
		a->crypto_kem_keypair(pk, sk);
		pk_list.push_back(pk);
		sk_list.push_back(sk);
		pk.clear();
		sk.clear();
	}
	return 0;
}

template<typename hashAlgo>
static vector<uint8_t> nested_dual_PRF(const vector<vector<uint8_t>> &list){
	vector<uint8_t> T = {};
	for(vector<uint8_t> e : list){
		T = HMAC<hashAlgo>(T, e);
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
static int crypto_enc_N(const list<shared_ptr<KEM>> &algo_list, int hash_id, vector<uint8_t> &ss, vector<vector<uint8_t>> &ct_list, const vector<vector<uint8_t>> &pk_list){
	vector<vector<uint8_t>> secret_list;
	vector<uint8_t> secret, ct;

	// Compute cipher text and secret for each algo
	int i=0;
	for(shared_ptr<KEM> e : algo_list){
		e->crypto_kem_enc(ct, secret, pk_list.at(i));
		secret_list.push_back(secret);
		ct_list.push_back(ct);
		bctbx_clean(secret.data(), secret.size());
		secret.clear();
		ct.clear();
		i++;
	}

	// Concatenation of the both cipher texts in ct
	for(vector<uint8_t> e : ct_list){
		ct.insert(ct.end(), e.begin(), e.end());
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
static int crypto_dec_N(const list<shared_ptr<KEM>> &algo_list, int hash_id, vector<uint8_t> &ss, const vector<vector<uint8_t>> &ct_list, const vector<vector<uint8_t>> &sk_list){
	vector<uint8_t> secret;
	vector<vector<uint8_t>> secret_list;

	// Compute secret for each algo
	int i = 0;
	for(shared_ptr<KEM> e : algo_list){
		e->crypto_kem_dec(secret, ct_list.at(i), sk_list.at(i));
		secret_list.push_back(secret);
		bctbx_clean(secret.data(), secret.size());
		secret.clear();
		i++;
	}

	// Concatenation of the both cipher texts in ct
	vector<uint8_t> ct;
	for(vector<uint8_t> e : ct_list){
		ct.insert(ct.end(), e.begin(), e.end());
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

/* HYBRID KEM INTERFACE */
// Use the combiner functions described before

int HYBRID_KEM::crypto_kem_keypair(vector<uint8_t> &pk, vector<uint8_t> &sk) const noexcept {
	int ret;
	vector<vector<uint8_t>> pkList;
	vector<vector<uint8_t>> skList;

	ret = crypto_gen_keypairs(algo, pkList, skList);

	for (vector<uint8_t> e : pkList) {
		pk.insert(pk.end(), e.begin(), e.end());
	}

	for (vector<uint8_t> e : skList) {
		sk.insert(sk.end(), e.begin(), e.end());
	}

	return ret;
}

int HYBRID_KEM::crypto_kem_enc(vector<uint8_t> &ct, vector<uint8_t> &ss, const vector<uint8_t> &pk) const noexcept {
	int ret;

	vector<vector<uint8_t>> ctList;
	vector<vector<uint8_t>> pkList;

	ctList.resize(algo.size());
	pkList.resize(algo.size());

	auto iter = pk.begin();
	int i = 0;
	for (shared_ptr<KEM> e : algo) {
		pkList.at(i).insert(pkList.at(i).end(), iter, iter+e->get_pkSize());
		iter += e->get_pkSize();
		i++;
	}

	ret = crypto_enc_N(algo, hash_id, ss, ctList, pkList);

	for (vector<uint8_t> e : ctList) {
		ct.insert(ct.end(), e.begin(), e.end());
	}

	return ret;
}

int HYBRID_KEM::crypto_kem_dec(vector<uint8_t> &ss, const vector<uint8_t> &ct, const vector<uint8_t> &sk) const noexcept {
	int ret;

	vector<vector<uint8_t>> ctList;
	vector<vector<uint8_t>> skList;

	ctList.resize(algo.size());
	skList.resize(algo.size());

	auto iter = sk.begin();
	auto iter2 = ct.begin();
	int i = 0;
	for (shared_ptr<KEM> e : algo) {
		skList.at(i).insert(skList.at(i).end(), iter, iter+e->get_skSize());
		iter += e->get_skSize();
		ctList.at(i).insert(ctList.at(i).end(), iter2, iter2+e->get_ctSize());
		iter2 += e->get_ctSize();
		i++;
	}

	ret = crypto_dec_N(algo, hash_id, ss, ctList, skList);

	return ret;
}

} /* namespace bctoolbox */


