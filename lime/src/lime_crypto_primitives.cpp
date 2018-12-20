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
#include "bctoolbox/exception.hh"

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

/***** Random Number Generator ********/
/**
 * @brief A wrapper around the bctoolbox Random Number Generator, implements the RNG interface
 */
class bctbx_RNG : public RNG {
	private :
		bctbx_rng_context_t *m_context; // the bctoolbox RNG context

		/* only bctbx_EDDSA and bctbx_ECDH needs a direct access to the actual RNG context */
		template <typename Curve> friend class bctbx_EDDSA;
		template <typename Curve> friend class bctbx_ECDH;

		/**
		 * @brief access internal RNG context
		 * Used internally by the bctoolbox wrapper, is not exposed to the lime_crypto_primitive API.
		 *
		 * @return a pointer to the RNG context
		 */
		bctbx_rng_context_t *get_context(void) {
			return m_context;
		}

	public:

		void randomize(sBuffer<lime::settings::DRrandomSeedSize> &buffer) override {
			bctbx_rng_get(m_context, buffer.data(), buffer.size());
		};

		uint32_t randomize() override {
			std::array<uint8_t, 4> buffer;
			bctbx_rng_get(m_context, buffer.data(), buffer.size());
			// we are on 31 bits: keep the uint32_t MSb set to 0 (see RNG interface definition)
			return (static_cast<uint32_t>(buffer[0]&0x7F)<<24 | static_cast<uint32_t>(buffer[1])<<16 | static_cast<uint32_t>(buffer[2])<<8 | static_cast<uint32_t>(buffer[3]));
		};

		bctbx_RNG() {
			m_context = bctbx_rng_context_new();
		}
	
		~bctbx_RNG() {
			bctbx_rng_context_free(m_context);
			m_context = nullptr;
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
			// the dynamic cast will generate an exception if RNG is not actually a bctbx_RNG
			bctbx_EDDSACreateKeyPair(m_context, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, dynamic_cast<lime::bctbx_RNG&>(*rng).get_context());
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
			// the dynamic cast will generate an exception if RNG is not actually a bctbx_RNG
			bctbx_ECDHCreateKeyPair(m_context, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, dynamic_cast<lime::bctbx_RNG&>(*rng).get_context());
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


/* Factory functions */
template <typename Curve>
std::shared_ptr<keyExchange<Curve>> make_keyExchange() {
	return std::make_shared<bctbx_ECDH<Curve>>();
}

template <typename Curve>
std::shared_ptr<Signature<Curve>> make_Signature() {
	return std::make_shared<bctbx_EDDSA<Curve>>();
}

/* HMAC templates */
/* HMAC must use a specialized template */
template <typename hashAlgo>
void HMAC(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
	static_assert(sizeof(hashAlgo) != sizeof(hashAlgo), "You must specialize HMAC_KDF function template");
}

/* HMAC specialized template for SHA512 */
template <> void HMAC<SHA512>(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize) {
	bctbx_hmacSha512(key, keySize, input, inputSize, static_cast<uint8_t>(std::min(SHA512::ssize(),hashSize)), hash);
}

/* generic implementation, of HKDF RFC-5869 */
template <typename hashAlgo, typename infoType>
void HMAC_KDF(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const infoType &info, uint8_t *output, size_t outputSize) {
	std::array<uint8_t, hashAlgo::ssize()> prk; // hold the output of pre-computation
	// extraction
	HMAC<hashAlgo>(salt, saltSize, ikm, ikmSize, prk.data(), prk.size());

	// expansion round 0
	std::vector<uint8_t> T(info.cbegin(), info.cend());
	T.push_back(0x01);
	HMAC<hashAlgo>(prk.data(), prk.size(), T.data(), T.size(), output, outputSize);

	// successives expansion rounds
	size_t index = std::min(outputSize, hashAlgo::ssize());
	for(uint8_t i=0x02; index < outputSize; i++) {
		T.assign(output+(i-2)*hashAlgo::ssize(), output+(i-1)*hashAlgo::ssize());
		T.insert(T.end(), info.cbegin(), info.cend());
		T.push_back(i);
		HMAC<hashAlgo>(prk.data(), prk.size(), T.data(), T.size(), output+index, outputSize-index);
		index += hashAlgo::ssize();
	}
	cleanBuffer(prk.data(), prk.size());
	cleanBuffer(T.data(), T.size());
}
template <typename hashAlgo, typename infoType>
void HMAC_KDF(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const infoType &info, uint8_t *output, size_t outputSize) {
	HMAC_KDF<hashAlgo>(salt.data(), salt.size(), ikm.data(), ikm.size(), info, output, outputSize);
};

/* instanciate HMAC_KDF template with SHA512 and string or vector info */
template void HMAC_KDF<SHA512, std::vector<uint8_t>>(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const std::vector<uint8_t> &info, uint8_t *output, size_t outputSize);
template void HMAC_KDF<SHA512, std::string>(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const std::string &info, uint8_t *output, size_t outputSize);
template void HMAC_KDF<SHA512, std::vector<uint8_t>>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::vector<uint8_t> &info, uint8_t *output, size_t outputSize);
template void HMAC_KDF<SHA512, std::string>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::string &info, uint8_t *output, size_t outputSize);

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
#endif //EC448_ENABLED


} // namespace lime


