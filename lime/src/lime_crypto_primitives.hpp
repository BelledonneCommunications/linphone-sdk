/*
	lime_crypto_primitives.hpp
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

#ifndef lime_crypto_primitives_hpp
#define lime_crypto_primitives_hpp

#include <memory>
#include <vector>

#include "lime_keys.hpp"

namespace lime {

class RNG {
	public:
		/**
		 * @Brief fill given buffer with Random bytes
		 *
		 * @param[out]	buffer	point to the beginning of the buffer to be filled with random bytes
		 * @param[in]	size	size of the buffer to be filled
		 */
		
		virtual void randomize(uint8_t *buffer, const size_t size) = 0;
		/**
		 * @Brief fill given buffer with Random bytes
		 *
		 * @param[out]	buffer	vector to be filled with random bytes(based on original vector size)
		 */
		virtual void randomize(std::vector<uint8_t> buffer) = 0;

		virtual ~RNG() = default;
}; //class RNG

/* Factory function */
std::shared_ptr<RNG> make_RNG();

template <typename Base>
class keyExchange {
	public:
		/* accessors */
		virtual const X<Base> get_secret(void) = 0; /**< Secret key */
		virtual const X<Base> get_selfPublic(void) = 0; /**< Self Public key */
		virtual const X<Base> get_peerPublic(void) = 0; /**< Peer Public key */
		virtual const X<Base> get_sharedSecret(void) = 0; /**< ECDH output */

		virtual void set_secret(const X<Base> &secret) = 0; /**< Secret key */
		virtual void set_selfPublic(const X<Base> &selfPublic) = 0; /**< Self Public key */
		virtual void set_peerPublic(const X<Base> &peerPublic) = 0; /**< Peer Public key */

		/**
		 * @Brief generate a new random key pair
		 *
		 * @param[in]	rng	The Random Number Generator to be used to generate the private kay
		 */
		virtual void createKeyPair(std::shared_ptr<lime::RNG> rng) = 0;

		/**
		 * @brief Compute the self public key using the secret already set in context
		 */
		virtual void deriveSelfPublic(void) = 0;

		/**
		 * @brief Perform the shared secret computation, it is then available in the object via get_sharedSecret
		 */
		virtual void computeSharedSecret(void) = 0;
		virtual ~keyExchange() = default;
}; //class keyExchange

/* Factory function */
template <typename Curve>
std::shared_ptr<keyExchange<Curve>> make_keyExchange();

template <typename Curve>
class EdDSA {
	public:
		/* accessors */
		virtual std::array<uint8_t, static_cast<size_t>(Curve::EDkeySize())> get_secret(void) = 0; /**< Secret key */
		virtual std::array<uint8_t, static_cast<size_t>(Curve::EDkeySize())> get_public(void) = 0; /**< Self Public key */

		virtual void set_secret(const std::array<uint8_t, static_cast<size_t>(Curve::EDkeySize())> &secret) = 0; /**< Secret key */
		virtual void set_public(const std::array<uint8_t, static_cast<size_t>(Curve::EDkeySize())> &selfPublic) = 0; /**< Self Public key */

		/**
		 * @Brief generate a new random EdDSA key pair
		 *
		 * @param[in]	rng	The Random Number Generator to be used to generate the private kay
		 */
		virtual void createKeyPair(std::shared_ptr<lime::RNG> rng) = 0;

		/**
		 * @brief Compute the public key using the secret already set in context
		 */
		virtual void derivePublic(void) = 0;

		/**
		 * @brief Sign a message using the key pair previously set in the object
		 *
		 * @param[in]	message		The message to be signed
		 * @param[in]	associatedData	A context for this signature, up to 255 bytes
		 * @param[out]	signature	The signature produced from the message with a key pair previously introduced in the object
		 */
		virtual void sign(const std::vector<uint8_t> &message, const std::vector<uint8_t> &associatedData, std::array<uint8_t, static_cast<size_t>(Curve::EDSigSize())> &signature) = 0;

		/**
		 * @brief Verify a message signature using the public key previously set in the object
		 *
		 * @param[in]	message		The message signed
		 * @param[in]	associatedData	A context for this signature, up to 255 bytes
		 * @param[in]	signature	The signature produced from the message with a key pair previously introduced in the object
		 *
		 * @return	true if the signature is valid, false otherwise
		 */
		virtual bool verify(const std::vector<uint8_t> &message, const std::vector<uint8_t> &associatedData, const std::array<uint8_t, static_cast<size_t>(Curve::EDSigSize())> &signature) = 0;

		/**
		 * @brief Key Format conversion(From EdDSA format to ECDH format)
		 *
		 * @param[out]	target	The ECDH object to store the key in
		 */
		virtual void convertPublicKeyToECDH(keyExchange<Curve> &target) = 0;
		virtual void convertPrivateKeyToECDH(keyExchange<Curve> &target) = 0;
		
		virtual ~EdDSA() = default;
}; //class EdDSA



/* this templates are instanciated once in the lime_crypto_primitives.cpp file, explicitly tell anyone including this header that there is no need to re-instanciate them */
#ifdef EC25519_ENABLED
	extern template std::shared_ptr<keyExchange<C255>> make_keyExchange();
#endif
#ifdef EC448_ENABLED
	extern template std::shared_ptr<keyExchange<C448>> make_keyExchange();
#endif

} // namespace lime

#endif //lime_crypto_primitives_hpp


