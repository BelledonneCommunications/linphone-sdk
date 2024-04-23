/*
	lime_crypto_primitives.cpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2017  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "lime_crypto_primitives.hpp"
#include "bctoolbox/crypto.h"
#include "bctoolbox/crypto.hh"
#include "bctoolbox/exception.hh"
#ifdef HAVE_BCTBXPQ
#include "postquantumcryptoengine/crypto.hh"
#endif /* HAVE_BCTBXPQ */

namespace lime {

/* template instanciations for Curves 25519 and 448, done  */
#if EC25519_ENABLED
	template class X<C255, lime::Xtype::publicKey>;
	template class X<C255, lime::Xtype::privateKey>;
	template class X<C255, lime::Xtype::sharedSecret>;
	template class Xpair<C255>;
	template class DSA<C255, lime::DSAtype::publicKey>;
	template class DSA<C255, lime::DSAtype::privateKey>;
	template class DSA<C255, lime::DSAtype::signature>;
	template class DSApair<C255>;
#endif

#if EC448_ENABLED
	template class X<C448, lime::Xtype::publicKey>;
	template class X<C448, lime::Xtype::privateKey>;
	template class X<C448, lime::Xtype::sharedSecret>;
	template class Xpair<C448>;
	template class DSA<C448, lime::DSAtype::publicKey>;
	template class DSA<C448, lime::DSAtype::privateKey>;
	template class DSA<C448, lime::DSAtype::signature>;
	template class DSApair<C448>;
#endif

#ifdef HAVE_BCTBXPQ
	template class K<K512, lime::Ktype::publicKey>;
	template class K<K512, lime::Ktype::privateKey>;
	template class K<K512, lime::Ktype::cipherText>;
	template class K<K512, lime::Ktype::sharedSecret>;
	template class Kpair<K512>;
#endif /* HAVE_BCTBXPQ */

/***** Random Number Generator ********/
/**
 * @brief A wrapper around the bctoolbox Random Number Generator, implements the RNG interface
 */
class bctbx_RNG : public RNG {
	private :
		bctoolbox::RNG m_context; // the bctoolbox RNG context

	public:
		uint32_t randomize() override {
			uint32_t ret = m_context.randomize();
			// we are on 31 bits: keep the uint32_t MSb set to 0 (see RNG interface definition)
			return (ret & 0x7FFFFFFF);
		};

		void randomize(uint8_t *buffer, const size_t size) override {
			m_context.randomize(buffer, size);
		}
}; // class bctbx_RNG

/* Factory function */
std::shared_ptr<RNG> make_RNG() {
	return std::make_shared<bctbx_RNG>();
}
/***** Signature  ********************/
/* bctbx_EdDSA specialized constructor */
template <typename Curve>
bctbx_EDDSAContext_t *bctbx_EDDSAInit(void) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
	static_assert(sizeof(Curve) != sizeof(Curve), "You must specialize Signature class constructor for your type");
	return nullptr;
}

#ifdef EC25519_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_EDDSAContext_t *bctbx_EDDSAInit<C255>(void) {
		return bctbx_CreateEDDSAContext(BCTBX_EDDSA_25519);
	}
#endif // EC25519_ENABLED

#ifdef EC448_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_EDDSAContext_t *bctbx_EDDSAInit<C448>(void) {
		return bctbx_CreateEDDSAContext(BCTBX_EDDSA_448);
	}
#endif // EC448_ENABLED

/**
 * @brief a wrapper around bctoolbox signature algorithms, implements the Signature interface
 *
 * Provides EdDSA on curves 25519 and 448
 */
template <typename Curve>
class bctbx_EDDSA : public Signature<Curve> {
	private :
		bctbx_EDDSAContext_t *m_context; // the EDDSA context
	public :
		/* accessors */
		const DSA<Curve, lime::DSAtype::privateKey> get_secret(void) override {
			if (m_context->secretKey == nullptr) {
				throw BCTBX_EXCEPTION << "invalid EdDSA secret key";
			}
			if (DSA<Curve, lime::DSAtype::privateKey>::ssize() != m_context->secretLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store EdDSA secret key";
			}
			DSA<Curve, lime::DSAtype::privateKey> s;
			std::copy_n(m_context->secretKey, s.ssize(), s.data());
			return s;
		}
		const DSA<Curve, lime::DSAtype::publicKey> get_public(void) override {
			if (m_context->publicKey == nullptr) {
				throw BCTBX_EXCEPTION << "invalid EdDSA public key";
			}
			if (DSA<Curve, lime::DSAtype::publicKey>::ssize() != m_context->pointCoordinateLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store EdDSA public key";
			}
			DSA<Curve, lime::DSAtype::publicKey> p;
			std::copy_n(m_context->publicKey, p.ssize(), p.data());
			return p;
		}

		/* Setting keys */
		void set_secret(const DSA<Curve, lime::DSAtype::privateKey> &secretKey) override {
			bctbx_EDDSA_setSecretKey(m_context, secretKey.data(), secretKey.ssize());
		}

		void set_public(const DSA<Curve, lime::DSAtype::publicKey> &publicKey) override {
			bctbx_EDDSA_setPublicKey(m_context, publicKey.data(), publicKey.ssize());
		}

		void createKeyPair(std::shared_ptr<lime::RNG> rng) override {
			// Generate a random secret key
			DSA<Curve, lime::DSAtype::privateKey> secret;
			rng->randomize(secret.data(), secret.size());
			// set it in the context
			set_secret(secret);
			// and generate the public value
			derivePublic();
		}

		void derivePublic(void) override {
			bctbx_EDDSADerivePublicKey(m_context);
		}

		void sign(const std::vector<uint8_t> &message, DSA<Curve, lime::DSAtype::signature> &signature) override {
			auto sigSize = signature.size();
			bctbx_EDDSA_sign(m_context, message.data(), message.size(), nullptr, 0, signature.data(), &sigSize);
		}

		void sign(const X<Curve, lime::Xtype::publicKey> &message, DSA<Curve, lime::DSAtype::signature> &signature) override {
			auto sigSize = signature.size();
			bctbx_EDDSA_sign(m_context, message.data(), message.ssize(), nullptr, 0, signature.data(), &sigSize);
		}

		bool verify(const std::vector<uint8_t> &message, const DSA<Curve, lime::DSAtype::signature> &signature) override {
			return (bctbx_EDDSA_verify(m_context, message.data(), message.size(), nullptr, 0, signature.data(), signature.size()) == BCTBX_VERIFY_SUCCESS);
		}

		bool verify(const X<Curve, lime::Xtype::publicKey> &message, const DSA<Curve, lime::DSAtype::signature> &signature) override {
			return (bctbx_EDDSA_verify(m_context, message.data(), message.ssize(), nullptr, 0, signature.data(), signature.ssize()) == BCTBX_VERIFY_SUCCESS);
		}

		bctbx_EDDSA() {
			m_context = bctbx_EDDSAInit<Curve>();
		}
		~bctbx_EDDSA(){
			/* perform proper destroy cleaning buffers*/
			bctbx_DestroyEDDSAContext(m_context);
			m_context = nullptr;
		}
}; // class bctbx_EDDSA

/***** Key Exchange ******************/

/* bctbx_ECDH specialized constructor */
template <typename Curve>
bctbx_ECDHContext_t *bctbx_ECDHInit(void) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
	static_assert(sizeof(Curve) != sizeof(Curve), "You must specialize keyExchange class contructor for your type");
	return nullptr;
}

#ifdef EC25519_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_ECDHContext_t *bctbx_ECDHInit<C255>(void) {
		return bctbx_CreateECDHContext(BCTBX_ECDH_X25519);
	}
#endif //EC25519_ENABLED

#ifdef EC448_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_ECDHContext_t *bctbx_ECDHInit<C448>(void) {
		return bctbx_CreateECDHContext(BCTBX_ECDH_X448);
	}
#endif //EC448_ENABLED

/**
 * @brief a wrapper around bctoolbox key exchange algorithms, implements the keyExchange interface
 *
 * Provides X25519 and X448
 */
template <typename Curve>
class bctbx_ECDH : public keyExchange<Curve> {
	private :
		bctbx_ECDHContext_t *m_context; // the ECDH context
	public :
		/* accessors */
		const X<Curve, lime::Xtype::privateKey> get_secret(void) override {
			if (m_context->secret == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH secret key";
			}
			if (X<Curve, lime::Xtype::privateKey>::ssize() != m_context->secretLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH secret key";
			}
			X<Curve, lime::Xtype::privateKey> s;
			std::copy_n(m_context->secret, s.ssize(), s.data());
			return s;
		}
		const X<Curve, lime::Xtype::publicKey> get_selfPublic(void) override {
			if (m_context->selfPublic == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH self public key";
			}
			if (X<Curve, lime::Xtype::publicKey>::ssize() != m_context->pointCoordinateLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH self public key";
			}
			X<Curve, lime::Xtype::publicKey> p;
			std::copy_n(m_context->selfPublic, p.ssize(), p.data());
			return p;
		}
		const X<Curve, lime::Xtype::publicKey> get_peerPublic(void) override {
			if (m_context->peerPublic == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH peer public key";
			}
			if (X<Curve, lime::Xtype::publicKey>::ssize() != m_context->pointCoordinateLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH peer public key";
			}
			X<Curve, lime::Xtype::publicKey> p;
			std::copy_n(m_context->peerPublic, p.ssize(), p.data());
			return p;
		}
		const X<Curve, lime::Xtype::sharedSecret> get_sharedSecret(void) override {
			if (m_context->sharedSecret == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH shared secret";
			}
			if (X<Curve, lime::Xtype::sharedSecret>::ssize() != m_context->pointCoordinateLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH output";
			}
			X<Curve, lime::Xtype::sharedSecret> s;
			std::copy_n(m_context->sharedSecret, s.ssize(), s.data());
			return s;
		}


		/* Setting keys, accept Signature keys */
		void set_secret(const X<Curve, lime::Xtype::privateKey> &secret) override {
			bctbx_ECDHSetSecretKey(m_context, secret.data(), secret.ssize());
		}

		void set_secret(const DSA<Curve, lime::DSAtype::privateKey> &secret) override {
			// we must create a temporary bctbx_EDDSA context and set the given key in
			auto tmp_context = bctbx_EDDSAInit<Curve>();
			bctbx_EDDSA_setSecretKey(tmp_context, secret.data(), secret.ssize());

			// Convert
			bctbx_EDDSA_ECDH_privateKeyConversion(tmp_context, m_context);

			// Cleaning
			bctbx_DestroyEDDSAContext(tmp_context);
		}

		void set_selfPublic(const X<Curve, lime::Xtype::publicKey> &selfPublic) override {
			bctbx_ECDHSetSelfPublicKey(m_context, selfPublic.data(), selfPublic.ssize());
		}

		void set_selfPublic(const DSA<Curve, lime::DSAtype::publicKey> &selfPublic) override {
			// we must create a temporary bctbx_EDDSA context and set the given key in
			auto tmp_context = bctbx_EDDSAInit<Curve>();
			bctbx_EDDSA_setPublicKey(tmp_context, selfPublic.data(), selfPublic.ssize());

			// Convert in self Public
			bctbx_EDDSA_ECDH_publicKeyConversion(tmp_context, m_context, BCTBX_ECDH_ISSELF);

			// Cleaning
			bctbx_DestroyEDDSAContext(tmp_context);
		}

		void set_peerPublic(const X<Curve, lime::Xtype::publicKey> &peerPublic) override {
			bctbx_ECDHSetPeerPublicKey(m_context, peerPublic.data(), peerPublic.ssize());
		}

		void set_peerPublic(const DSA<Curve, lime::DSAtype::publicKey> &peerPublic) override {
			// we must create a temporary bctbx_EDDSA context and set the given key in
			auto tmp_context = bctbx_EDDSAInit<Curve>();
			bctbx_EDDSA_setPublicKey(tmp_context, peerPublic.data(), peerPublic.ssize());

			// Convert in peer Public
			bctbx_EDDSA_ECDH_publicKeyConversion(tmp_context, m_context, BCTBX_ECDH_ISPEER);

			// Cleaning
			bctbx_DestroyEDDSAContext(tmp_context);
		}

		void createKeyPair(std::shared_ptr<lime::RNG> rng) override {
			// Generate a random secret key
			X<Curve, lime::Xtype::privateKey> secret;
			rng->randomize(secret.data(), secret.size());
			// set it in the context
			set_secret(secret);
			// and generate the public value
			deriveSelfPublic();
		}

		void deriveSelfPublic(void) override {
			bctbx_ECDHDerivePublicKey(m_context);
		}

		void computeSharedSecret(void) override {
			 bctbx_ECDHComputeSecret(m_context, nullptr, nullptr);
		}

		bctbx_ECDH() {
			m_context = bctbx_ECDHInit<Curve>();
		}
		~bctbx_ECDH(){
			/* perform proper destroy cleaning buffers*/
			bctbx_DestroyECDHContext(m_context);
			m_context = nullptr;
		}
}; // class bctbx_ECDH

/* bctbx_KEM specialized constructor */
#ifdef HAVE_BCTBXPQ
template <typename Algo>
std::unique_ptr<bctoolbox::KEM> bctbx_KEMInit(void) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
	static_assert(sizeof(Algo) != sizeof(Algo), "You must specialize KEM class contructor for your type");
	return nullptr;
}

	/* specialise KEM creation : K512 is Kyber512 */
	template <> std::unique_ptr<bctoolbox::KEM> bctbx_KEMInit<K512>(void) {
		return std::make_unique<bctoolbox::KYBER512>();
	}

/**
 * @brief a wrapper around bctoolbox KEM algorithms, implements the key encapsulation mechanism interface
 *
 * Provides hybrid KEM curve 25519/Kyber512
 */
template <typename Algo>
class bctbx_KEM : public KEM<Algo> {
	private :
		std::unique_ptr<bctoolbox::KEM> m_ctx;
	public:
		void createKeyPair(Kpair<Algo>&keyPair) override {
			std::vector<uint8_t> pk,sk;
			m_ctx->crypto_kem_keypair(pk,sk);
			keyPair.publicKey().assign(pk.cbegin());
			keyPair.privateKey().assign(sk.cbegin());
			cleanBuffer(sk.data(), sk.size());	
		}
		void encaps(const K<Algo, lime::Ktype::publicKey> &publicKey, K<Algo, lime::Ktype::cipherText> &cipherText, K<Algo, lime::Ktype::sharedSecret> &sharedSecret) override {
			std::vector<uint8_t> ct,ss;
			std::vector<uint8_t> pk(publicKey.cbegin(), publicKey.cend());
			m_ctx->crypto_kem_enc(ct, ss, pk);
			cipherText.assign(ct.cbegin());
			sharedSecret.assign(ss.cbegin());
			cleanBuffer(ss.data(), ss.size());
		}
		void decaps(const K<Algo, lime::Ktype::privateKey> &privateKey, const K<Algo, lime::Ktype::cipherText> &cipherText, K<Algo, lime::Ktype::sharedSecret> &sharedSecret) override {
			std::vector<uint8_t> ss;
			std::vector<uint8_t> sk(privateKey.cbegin(), privateKey.cend());
			std::vector<uint8_t> ct(cipherText.cbegin(), cipherText.cend());
			m_ctx->crypto_kem_dec(ss, ct, sk);
			sharedSecret.assign(ss.cbegin());
			cleanBuffer(ss.data(), ss.size());
			cleanBuffer(sk.data(), sk.size());
		}
		bctbx_KEM() {
			m_ctx = bctbx_KEMInit<K512>();
		}
}; // class bctbx_KEM
#endif //HAVE_BCTBXPQ

/* Factory functions */
template <typename Curve>
std::shared_ptr<keyExchange<Curve>> make_keyExchange() {
	return std::make_shared<bctbx_ECDH<Curve>>();
}

template <typename Curve>
std::shared_ptr<Signature<Curve>> make_Signature() {
	return std::make_shared<bctbx_EDDSA<Curve>>();
}

#ifdef HAVE_BCTBXPQ
template <typename Algo>
std::shared_ptr<KEM<Algo>> make_KEM() {
	return std::make_shared<bctbx_KEM<Algo>>();
}
#endif //HAVE_BCTBXPQ

/* HMAC templates */
/* HMAC must use a specialized template */
template <typename hashAlgo>
void HMAC(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
	static_assert(sizeof(hashAlgo) != sizeof(hashAlgo), "You must specialize HMAC function template");
}

/* HMAC specialized template for SHA512 */
template <> void HMAC<SHA512>(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize) {
	bctbx_hmacSha512(key, keySize, input, inputSize, static_cast<uint8_t>(std::min(SHA512::ssize(),hashSize)), hash);
}

/* HMAC must use a specialized template */
template <typename hashAlgo> void HMAC_KDF(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const char *info, const size_t infoSize, uint8_t *output, size_t outputSize) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
	static_assert(sizeof(hashAlgo) != sizeof(hashAlgo), "You must specialize HMAC_KDF function template");
}
/* HMAC_KDF specialised template for SHA512 */
template <> void HMAC_KDF<SHA512>(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const char *info, const size_t infoSize, uint8_t *output, size_t outputSize) {
	bctoolbox::HKDF<bctoolbox::SHA512>(salt, saltSize, ikm, ikmSize, info, infoSize, output, outputSize);
};

/* AEAD template must be specialized */
template <typename AEADAlgo>
void AEAD_encrypt(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const plain, const size_t plainSize, const uint8_t *const AD, const size_t ADSize,
		uint8_t *tag, const size_t tagSize, uint8_t *cipher) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty type */
	static_assert(sizeof(AEADAlgo) != sizeof(AEADAlgo), "You must specialize AEAD_encrypt function template");
}

template <typename AEADAlgo>
bool AEAD_decrypt(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const cipher, const size_t cipherSize, const uint8_t *const AD, const size_t ADSize,
		const uint8_t *const tag, const size_t tagSize, uint8_t *plain) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty type */
	static_assert(sizeof(AEADAlgo) != sizeof(AEADAlgo), "You must specialize AEAD_decrypt function template");
	return false;
}

/* AEAD scheme specialiazed template with AES256-GCM, 16 bytes auth tag */
template <> void AEAD_encrypt<AES256GCM>(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const plain, const size_t plainSize, const uint8_t *const AD, const size_t ADSize,
		uint8_t *tag, const size_t tagSize, uint8_t *cipher) {
	/* perforn checks on sizes */
	if (keySize != AES256GCM::keySize() || tagSize != AES256GCM::tagSize()) {
		throw BCTBX_EXCEPTION << "invalid arguments for AEAD_encrypt AES256-GCM";
	}
	auto ret = bctbx_aes_gcm_encrypt_and_tag(key, keySize, plain, plainSize, AD, ADSize, IV, IVSize, tag, tagSize, cipher);
	if (ret != 0) {
		throw BCTBX_EXCEPTION << "AEAD_encrypt AES256-GCM error: "<<ret;
	}
}

template <> bool AEAD_decrypt<AES256GCM>(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const cipher, const size_t cipherSize, const uint8_t *const AD, const size_t ADSize,
		const uint8_t *const tag, const size_t tagSize, uint8_t *plain) {
	/* perforn checks on sizes */
	if (keySize != AES256GCM::keySize() || tagSize != AES256GCM::tagSize()) {
		throw BCTBX_EXCEPTION << "invalid arguments for AEAD_decrypt AES256-GCM";
	}
	auto ret = bctbx_aes_gcm_decrypt_and_auth(key, keySize, cipher, cipherSize, AD, ADSize, IV, IVSize, tag, tagSize, plain);
	if (ret == 0) return true;
	if (ret == BCTBX_ERROR_AUTHENTICATION_FAILED) return false;
	throw BCTBX_EXCEPTION << "AEAD_decrypt AES256-GCM error: "<<ret;
}

/* check buffer length are in sync with bctoolbox ones */
#ifdef EC25519_ENABLED
	static_assert(BCTBX_ECDH_X25519_PUBLIC_SIZE == X<C255, Xtype::publicKey>::ssize(), "bctoolbox and local defines mismatch");
	// for ECDH public value and shared secret have the same size
	static_assert(BCTBX_ECDH_X25519_PUBLIC_SIZE == X<C255, Xtype::sharedSecret>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(BCTBX_ECDH_X25519_PRIVATE_SIZE == X<C255, Xtype::privateKey>::ssize(), "bctoolbox and local defines mismatch");

	static_assert(BCTBX_EDDSA_25519_PUBLIC_SIZE == DSA<C255, DSAtype::publicKey>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(BCTBX_EDDSA_25519_PRIVATE_SIZE == DSA<C255, DSAtype::privateKey>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(BCTBX_EDDSA_25519_SIGNATURE_SIZE == DSA<C255, DSAtype::signature>::ssize(), "bctoolbox and local defines mismatch");
#endif //EC25519_ENABLED

#ifdef EC448_ENABLED
	static_assert(BCTBX_ECDH_X448_PUBLIC_SIZE == X<C448, Xtype::publicKey>::ssize(), "bctoolbox and local defines mismatch");
	// for ECDH public value and shared secret have the same size
	static_assert(BCTBX_ECDH_X448_PUBLIC_SIZE == X<C448, Xtype::sharedSecret>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(BCTBX_ECDH_X448_PRIVATE_SIZE == X<C448, Xtype::privateKey>::ssize(), "bctoolbox and local defines mismatch");

	static_assert(BCTBX_EDDSA_448_PUBLIC_SIZE == DSA<C448, DSAtype::publicKey>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(BCTBX_EDDSA_448_PRIVATE_SIZE == DSA<C448, DSAtype::privateKey>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(BCTBX_EDDSA_448_SIGNATURE_SIZE == DSA<C448, DSAtype::signature>::ssize(), "bctoolbox and local defines mismatch");
#endif //EC448_ENABLED

#ifdef HAVE_BCTBXPQ
	static_assert(bctoolbox::KYBER512::pkSize == K<K512, Ktype::publicKey>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(bctoolbox::KYBER512::skSize == K<K512, Ktype::privateKey>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(bctoolbox::KYBER512::ctSize == K<K512, Ktype::cipherText>::ssize(), "bctoolbox and local defines mismatch");
	static_assert(bctoolbox::KYBER512::ssSize == K<K512, Ktype::sharedSecret>::ssize(), "bctoolbox and local defines mismatch");
#endif //HAVE_BCTBXPQ
/**
 * @brief force a buffer values to zero in a way that shall prevent the compiler from optimizing it out
 *
 * @param[in,out]	buffer	the buffer to be cleared
 * @param[in]		size	buffer size
 */
void cleanBuffer(uint8_t *buffer, size_t size) {
	bctbx_clean(buffer, size);
}

/* template instanciations for Curve 25519 and Curve 448 */
#ifdef EC25519_ENABLED
	template class bctbx_ECDH<C255>;
	template class bctbx_EDDSA<C255>;
	template std::shared_ptr<keyExchange<C255>> make_keyExchange();
	template std::shared_ptr<Signature<C255>> make_Signature();
#endif //EC25519_ENABLED

#ifdef EC448_ENABLED
	template class bctbx_ECDH<C448>;
	template class bctbx_EDDSA<C448>;
	template std::shared_ptr<keyExchange<C448>> make_keyExchange();
	template std::shared_ptr<Signature<C448>> make_Signature();
#endif //EC448_ENAB
#ifdef HAVE_BCTBXPQ
	template class bctbx_KEM<K512>;
	template std::shared_ptr<KEM<K512>> make_KEM();
#endif //HAVE_BCTBXPQ
} // namespace lime
