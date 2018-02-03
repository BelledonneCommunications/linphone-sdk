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

/***** Random Number Generator ********/
class bctbx_RNG : public RNG {
	private :
		bctbx_rng_context_t *m_context; // the bctoolbox RNG context

	public:
		/* accessor */
		bctbx_rng_context_t *get_context(void) {
			return m_context;
		}

		void randomize(uint8_t *buffer, const size_t size) override {
			bctbx_rng_get(m_context, buffer, size);
		};
		void randomize(std::vector<uint8_t> buffer) override {
			randomize(buffer.data(), buffer.size());
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

/***** Key Exchange ******************/

/* bctbx_ECDH specialized constructor */
template <typename Curve>
bctbx_ECDHContext_t *bctbx_ECDHInit(void) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty Curve type */
	static_assert(sizeof(Curve) != sizeof(Curve), "You must specialize sessionsInit<> for your type: correctly initialise the ECDH context");
	return nullptr;
}

#ifdef EC25519_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_ECDHContext_t *bctbx_ECDHInit<C255>(void) {
		return bctbx_CreateECDHContext(BCTBX_ECDH_X25519);
	}
#endif

#ifdef EC448_ENABLED
	/* specialise ECDH context creation */
	template <> bctbx_ECDHContext_t *bctbx_ECDHInit<C448>(void) {
		return bctbx_CreateECDHContext(BCTBX_ECDH_X448);
	}
#endif

template <typename Curve>
class bctbx_ECDH : public keyExchange<Curve> {
	private :
		bctbx_ECDHContext_t *m_context; // the ECDH RNG context
	public :
		/* accessors */
		const X<Curve> get_secret(void) override { /**< Secret key */
			if (m_context->secret == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH secret key";
			}
			if (X<Curve>::keyLength() != m_context->secretLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH secret key";
			}
			X<Curve> s;
			std::copy_n(m_context->secret, s.keyLength(), s.data());
			return s;
		}
		const X<Curve> get_selfPublic(void) override {/**< Self Public key */
			if (m_context->selfPublic == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH self public key";
			}
			if (X<Curve>::keyLength() != m_context->pointCoordinateLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH self public key";
			}
			X<Curve> p;
			std::copy_n(m_context->selfPublic, p.keyLength(), p.data());
			return p;
		}
		const X<Curve> get_peerPublic(void) override { /**< Peer Public key */
			if (m_context->peerPublic == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH peer public key";
			}
			if (X<Curve>::keyLength() != m_context->pointCoordinateLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH peer public key";
			}
			X<Curve> p;
			std::copy_n(m_context->peerPublic, p.keyLength(), p.data());
			return p;
		}
		const X<Curve> get_sharedSecret(void) override { /**< ECDH output */
			if (m_context->sharedSecret == nullptr) {
				throw BCTBX_EXCEPTION << "invalid ECDH shared secret";
			}
			if (X<Curve>::keyLength() != m_context->pointCoordinateLength) {
				throw BCTBX_EXCEPTION << "Invalid buffer to store ECDH output";
			}
			X<Curve> s;
			std::copy_n(m_context->sharedSecret, s.keyLength(), s.data());
			return s;
		}

		void set_secret(const X<Curve> &secret) override { /**< Secret key */
			bctbx_ECDHSetSecretKey(m_context, secret.data(), secret.keyLength());
		}

		void set_selfPublic(const X<Curve> &selfPublic) override { /**< Self Public key */
			bctbx_ECDHSetSelfPublicKey(m_context, selfPublic.data(), selfPublic.keyLength()); 
		}
		void set_peerPublic(const X<Curve> &peerPublic) override {; /**< Peer Public key */
			bctbx_ECDHSetPeerPublicKey(m_context, peerPublic.data(), peerPublic.keyLength());
		}


		/**
		 * @Brief generate a new random ECDH key pair
		 *
		 * @param[in]	rng	The Random Number Generator to be used to generate the private kay
		 */
		void createKeyPair(std::shared_ptr<lime::RNG> rng) override {
			// the dynamic cast will generate an exception if RNG is not actually a bctbx_RNG
			bctbx_ECDHCreateKeyPair(m_context, (int (*)(void *, uint8_t *, size_t))bctbx_rng_get, dynamic_cast<lime::bctbx_RNG&>(*rng).get_context());
		}

		/**
		 * @brief Compute the self public key using the secret already set in context
		 */
		void deriveSelfPublic(void) override {
			bctbx_ECDHDerivePublicKey(m_context);
		}

		/**
		 * @brief Perform the ECDH computation, shared secret is then available in the object via get_sharedSecret
		 */
		void computeSharedSecret(void) override {
			 bctbx_ECDHComputeSecret(m_context, nullptr, nullptr);
		}

		/**
		 * ctor/dtor
		 */
		bctbx_ECDH() {
			m_context = bctbx_ECDHInit<Curve>();
		}
		~bctbx_ECDH(){
			/* perform proper destroy cleaning buffers*/
			bctbx_DestroyECDHContext(m_context);
			m_context = nullptr;
		}
}; // class bctbx_ECDH

/* Factory function */
template <typename Base>
std::shared_ptr<keyExchange<Base>> make_keyExchange() {
	return std::make_shared<bctbx_ECDH<Base>>();
}


	/* template instanciations for Curve 25519 and Curve 448 */
#ifdef EC25519_ENABLED
	template class bctbx_ECDH<C255>;
	template std::shared_ptr<keyExchange<C255>> make_keyExchange();
#endif

#ifdef EC448_ENABLED
	template class bctbx_ECDH<C448>;
	template std::shared_ptr<keyExchange<C448>> make_keyExchange();
#endif


/***** Signature  ********************/

} // namespace lime


