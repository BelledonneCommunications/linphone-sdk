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
	/* auto clean fixed size buffer                                 */
	/****************************************************************/
	template <size_t T>
	struct sBuffer : public std::array<uint8_t, T> {
			~sBuffer() {cleanBuffer(this->data(), T);}; // zeroise all buffer when done
	};


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

/**
 * @brief templated HMAC
 *	template parameter is the hash algorithm used (SHA512 available for now)
 *
 * @parameter[in]	key
 * @parameter[in]	keySize		previous buffer size
 * @parameter[in]	input
 * @parameter[in]	inputSize	previous buffer size
 * @parameter[out]	hash		pointer to the output, this buffer must be able to hold as much data as requested
 * @parameter[in]	hashSize	amount of expected data, if more than selected Hash algorithm can compute, silently ignored and maximum output size is generated
 *
 */
template <typename hashAlgo>
void HMAC(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize);
/* declare template specialisations */
template <> void HMAC<SHA512>(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize);

/**
 * @brief HKDF as described in RFC5869
 *	template parameters:
 *	hashAlgo: the hash algorithm to use (SHA512 available for now)
 *	infoType: the info parameter can be passed as a string or a std::vector<uint8_t>
 *	Compute:
 *		PRK = HMAC-Hash(salt, IKM)
 *
 *		N = ceil(L/HashLen)
 *		T = T(1) | T(2) | T(3) | ... | T(N)
 *		OKM = first L octets of T
 *
 *		where:
 *		T(0) = empty string (zero length)
 *		T(1) = HMAC-Hash(PRK, T(0) | info | 0x01)
 *		T(2) = HMAC-Hash(PRK, T(1) | info | 0x02)
 *		T(3) = HMAC-Hash(PRK, T(2) | info | 0x03)
 *		...
 *
 * @param[in]	salt 		salt
 * @param[in]	ikm		input key material
 * @param[in]	info		a info string or buffer
 * @param[out]	okm		output key material
 * @param[in]	okmSize		requested amount of data, okm buffer must be able to hold it. (L in the RFC doc)
 *
 */
template <typename hashAlgo, typename infoType>
void HMAC_KDF(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const infoType &info, uint8_t *okm, size_t okmSize);
template <typename hashAlgo, typename infoType>
void HMAC_KDF(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const infoType &info, uint8_t *output, size_t outputSize);


/************************ AEAD interface *************************************/

/**
 * @Brief Encrypt and tag using scheme given as template parameter
 *
 * @param[in]	key		Encryption key
 * @param[in]	keySize		Key buffer length, it must match the selected AEAD scheme or an exception is generated
 * @param[in]	IV		Buffer holding the initialisation vector
 * @param[in]	IVSize		Initialisation vector length in bytes
 * @param[in]	plain		buffer to be encrypted
 * @param[in]	plainSize	Length in bytes of buffer to be encrypted
 * @param[in]	AD		Buffer holding additional data to be used in tag computation
 * @param[in]	ADSize		Additional data length in bytes
 * @param[out]	tag		Buffer holding the generated tag
 * @param[in]	tagSize		Requested length for the generated tag, it must match the selected AEAD scheme or an exception is generated
 * @param[out]	cipher		Buffer holding the output, shall be at least the length of plainText buffer
 */
template <typename AEADAlgo>
void AEAD_encrypt(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const plain, const size_t plainSize, const uint8_t *const AD, const size_t ADSize,
		uint8_t *tag, const size_t tagSize, uint8_t *cipher);

/**
 * @Brief Authenticate and Decrypt using scheme given as template parameter
 *
 * @param[in]	key		Encryption key
 * @param[in]	keySize		Key buffer length, it must match the selected AEAD scheme or an exception is generated
 * @param[in]	IV		Buffer holding the initialisation vector
 * @param[in]	IVSize		Initialisation vector length in bytes
 * @param[in]	cipher		Buffer holding the data to be decipherd
 * @param[in]	plainSize	Length in bytes of buffer to be encrypted
 * @param[in]	AD		Buffer holding additional data to be used in tag computation
 * @param[in]	ADSize		Additional data length in bytes
 * @param[in]	tag		Buffer holding the authentication tag
 * @param[in]	tagSize		Length for the generated tag, it must match the selected AEAD scheme or an exception is generated
 * @param[out]	plain		buffer holding the plain output, shall be at least the length of plainText buffer
 *
 * @return true if authentication tag match and decryption was successful
 */
template <typename AEADAlgo>
bool AEAD_decrypt(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const cipher, const size_t cipherSize, const uint8_t *const AD, const size_t ADSize,
		const uint8_t *const tag, const size_t tagSize, uint8_t *plain);

/* declare AEAD template specialisations */
template <> void AEAD_encrypt<AES256GCM>(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const plain, const size_t plainSize, const uint8_t *const AD, const size_t ADSize,
		uint8_t *tag, const size_t tagSize, uint8_t *cipher);

template <> bool AEAD_decrypt<AES256GCM>(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const cipher, const size_t cipherSize, const uint8_t *const AD, const size_t ADSize,
		const uint8_t *const tag, const size_t tagSize, uint8_t *plain);


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
extern template void HMAC_KDF<SHA512, std::vector<uint8_t>>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::vector<uint8_t> &info, uint8_t *output, size_t outputSize);
extern template void HMAC_KDF<SHA512, std::string>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::string &info, uint8_t *output, size_t outputSize);
extern template void HMAC_KDF<SHA512, std::vector<uint8_t>>(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const std::vector<uint8_t> &info, uint8_t *output, size_t outputSize);
extern template void HMAC_KDF<SHA512, std::string>(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const std::string &info, uint8_t *output, size_t outputSize);

extern template void AEAD_encrypt<AES256GCM>(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const plain, const size_t plainSize, const uint8_t *const AD, const size_t ADSize,
		uint8_t *tag, const size_t tagSize, uint8_t *cipher);

extern template bool AEAD_decrypt<AES256GCM>(const uint8_t *const key, const size_t keySize, const uint8_t *const IV, const size_t IVSize,
		const uint8_t *const cipher, const size_t cipherSize, const uint8_t *const AD, const size_t ADSize,
		const uint8_t *const tag, const size_t tagSize, uint8_t *plain);

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


