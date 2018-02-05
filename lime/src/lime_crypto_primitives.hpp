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
/*************************************************************************************************/
/********************** Data Structures **********************************************************/
/*************************************************************************************************/

	/**
	 * @brief force a buffer values to zero in a way that shall prevent the compiler from optimizing it out
	 *
	 * @param[in/out]	buffer	the buffer to be cleared
	 * @param[in]		size	buffer size
	 */
	void cleanBuffer(uint8_t *buffer, size_t size);

	/****************************************************************/
	/* Key Exchange Data structures                                 */
	/****************************************************************/
	template <typename Base, lime::Xtype dataType>
	class X : public std::array<uint8_t, static_cast<size_t>(Base::Xsize(dataType))>{
		public :
			constexpr static size_t ssize(void) {return Base::Xsize(dataType);}; // provide a static size function to be able to call the function not on an object
			X(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Base::Xsize(dataType), this->begin());} // construct from a std::vector<uint8_t>
			X() {};
			void assign(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Base::Xsize(dataType), this->begin());} // copy from a std::vector<uint8_t>
			~X() {cleanBuffer(this->data(), Base::Xsize(dataType));}; // zeroise all buffer when done
	};

	template <typename Base>
	class Xpair {
		private:
			X<Base, lime::Xtype::publicKey> m_pubKey;
			X<Base, lime::Xtype::privateKey> m_privKey;
		public:
			X<Base, lime::Xtype::privateKey> &privateKey(void) {return m_privKey;};
			X<Base, lime::Xtype::publicKey> &publicKey(void) {return m_pubKey;};
			Xpair(X<Base, lime::Xtype::publicKey> &pub, X<Base, lime::Xtype::privateKey> &priv):m_pubKey(pub),m_privKey(priv) {};
			Xpair() :m_pubKey{},m_privKey{}{};
			bool operator==(Xpair<Base> b) const {return (m_privKey==b.privateKey() && m_pubKey==b.publicKey());};

	};

	/****************************************************************/
	/* Digital Signature Algorithm data structures                  */
	/****************************************************************/
	template <typename Base, lime::DSAtype dataType>
	class DSA : public std::array<uint8_t, static_cast<size_t>(Base::DSAsize(dataType))>{
		public :
			constexpr static size_t ssize(void) {return Base::DSAsize(dataType);}; // provide a static size function to be able to call the function not on an object
			DSA(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Base::DSAsize(dataType), this->begin());} // contruct from a std::vector<uint8_t>
			DSA() {};
			void assign(std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, Base::DSAsize(dataType), this->begin());} // copy from a std::vector<uint8_t>
			~DSA() {cleanBuffer(this->data(), Base::DSAsize(dataType));}; // zeroise all buffer when done
	};

	template <typename Base>
	class DSApair {
		private:
			DSA<Base, lime::DSAtype::publicKey> m_pubKey;
			DSA<Base, lime::DSAtype::privateKey> m_privKey;
		public:
			DSA<Base, lime::DSAtype::privateKey> &privateKey(void) {return m_privKey;};
			DSA<Base, lime::DSAtype::publicKey> &publicKey(void) {return m_pubKey;};
			DSApair(DSA<Base, lime::DSAtype::publicKey> &pub, DSA<Base, lime::DSAtype::privateKey> &priv):m_pubKey(pub),m_privKey(priv) {};
			DSApair() :m_pubKey{},m_privKey{}{};
			bool operator==(DSApair<Base> b) const {return (m_privKey==b.privateKey() && m_pubKey==b.publicKey());};
	};


/*************************************************************************************************/
/********************** Crypto Algo interface ****************************************************/
/*************************************************************************************************/

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

template <typename Base>
class keyExchange {
	public:
		/* accessors */
		virtual const X<Base, lime::Xtype::privateKey> get_secret(void) = 0; /**< Secret key */
		virtual const X<Base, lime::Xtype::publicKey> get_selfPublic(void) = 0; /**< Self Public key */
		virtual const X<Base, lime::Xtype::publicKey> get_peerPublic(void) = 0; /**< Peer Public key */
		virtual const X<Base, lime::Xtype::sharedSecret> get_sharedSecret(void) = 0; /**< ECDH output */

		/* set keys in context, publics and private keys directly accept Signature formatted keys which are converted to keyExchange format */
		virtual void set_secret(const X<Base, lime::Xtype::privateKey> &secret) = 0; /**< Secret key */
		virtual void set_secret(const DSA<Base, lime::DSAtype::privateKey> &secret) = 0; /**< Secret key */
		virtual void set_selfPublic(const X<Base, lime::Xtype::publicKey> &selfPublic) = 0; /**< Self Public key */
		virtual void set_selfPublic(const DSA<Base, lime::DSAtype::publicKey> &selfPublic) = 0; /**< Self Public key */
		virtual void set_peerPublic(const X<Base, lime::Xtype::publicKey> &peerPublic) = 0; /**< Peer Public key */
		virtual void set_peerPublic(const DSA<Base, lime::DSAtype::publicKey> &peerPublic) = 0; /**< Peer Public key */

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

template <typename Base>
class Signature {
	public:
		/* accessors */
		virtual const DSA<Base, lime::DSAtype::privateKey> get_secret(void) = 0; /**< Secret key */
		virtual const DSA<Base, lime::DSAtype::publicKey> get_public(void) = 0; /**< Self Public key */

		virtual void set_secret(const DSA<Base, lime::DSAtype::privateKey> &secretKey) = 0; /**< Secret key */
		virtual void set_public(const DSA<Base, lime::DSAtype::publicKey> &publicKey) = 0; /**< Self Public key */

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
		 * @param[in]	message		The message to be signed (we can sign any vector or more specifically a key exchange public key)
		 * @param[out]	signature	The signature produced from the message with a key pair previously introduced in the object
		 */
		virtual void sign(const std::vector<uint8_t> &message, DSA<Base, lime::DSAtype::signature> &signature) = 0;
		virtual void sign(const X<Base, lime::Xtype::publicKey> &message, DSA<Base, lime::DSAtype::signature> &signature) = 0;

		/**
		 * @brief Verify a message signature using the public key previously set in the object
		 *
		 * @param[in]	message		The message signed (we can verify any vector or more specifically a key exchange public key)
		 * @param[in]	signature	The signature produced from the message with a key pair previously introduced in the object
		 *
		 * @return	true if the signature is valid, false otherwise
		 */
		virtual bool verify(const std::vector<uint8_t> &message, const DSA<Base, lime::DSAtype::signature> &signature) = 0;
		virtual bool verify(const X<Base, lime::Xtype::publicKey> &message, const DSA<Base, lime::DSAtype::signature> &signature) = 0;

		virtual ~Signature() = default;
}; //class EdDSA


/*************************************************************************************************/
/********************** Factory Functions ********************************************************/
/*************************************************************************************************/
/* Use these to instantiate an object as they will pick the correct undurlying implemenation of virtual classes */
std::shared_ptr<RNG> make_RNG();

template <typename Base>
std::shared_ptr<keyExchange<Base>> make_keyExchange();

template <typename Base>
std::shared_ptr<Signature<Base>> make_Signature();


/*************************************************************************************************/
/********************** Template Instanciation ***************************************************/
/*************************************************************************************************/
/* this templates are instanciated once in the lime_crypto_primitives.cpp file, explicitly tell anyone including this header that there is no need to re-instanciate them */
#ifdef EC25519_ENABLED
	extern template std::shared_ptr<keyExchange<C255>> make_keyExchange();
	extern template std::shared_ptr<Signature<C255>> make_Signature();
	extern template class X<C255, lime::Xtype::publicKey>;
	extern template class X<C255, lime::Xtype::privateKey>;
	extern template class X<C255, lime::Xtype::sharedSecret>;
	extern template class Xpair<C255>;
	extern template class DSA<C255, lime::DSAtype::publicKey>;
	extern template class DSA<C255, lime::DSAtype::privateKey>;
	extern template class DSA<C255, lime::DSAtype::signature>;
	extern template class DSApair<C255>;
#endif // EC25519_ENABLED

#ifdef EC448_ENABLED
	extern template std::shared_ptr<keyExchange<C448>> make_keyExchange();
	extern template std::shared_ptr<Signature<C448>> make_Signature();
	extern template class X<C448, lime::Xtype::publicKey>;
	extern template class X<C448, lime::Xtype::privateKey>;
	extern template class X<C448, lime::Xtype::sharedSecret>;
	extern template class Xpair<C448>;
	extern template class DSA<C448, lime::DSAtype::publicKey>;
	extern template class DSA<C448, lime::DSAtype::privateKey>;
	extern template class DSA<C448, lime::DSAtype::signature>;
	extern template class DSApair<C448>;
#endif // EC448_ENABLED

} // namespace lime

#endif //lime_crypto_primitives_hpp


