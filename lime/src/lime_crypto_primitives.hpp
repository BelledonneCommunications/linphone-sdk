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
#include "lime_defines.hpp"

namespace lime {
/*************************************************************************************************/
/********************** Data Structures **********************************************************/
/*************************************************************************************************/

	void cleanBuffer(uint8_t *buffer, size_t size);

	/**
	 * @brief auto clean fixed size buffer(std::array based)
	 *
	 * Buffers are cleaned when destroyed, use this to store any sensitive data
	 */
	template <size_t T>
	struct sBuffer : public std::array<uint8_t, T> {
		/// zeroise all buffer when done
		~sBuffer() {cleanBuffer(this->data(), T);};
	};

	/****************************************************************/
	/* Key Exchange Data structures                                 */
	/****************************************************************/
	/**
	 * @brief Base buffer definition for Key Exchange data structure
	 *
	 * easy use of array types with correct size
	 */
	template <typename Curve, lime::Xtype dataType>
	class X : public sBuffer<static_cast<size_t>(Curve::Xsize(dataType))>{
		public :
			/// provide a static size function to be able to call the function not on an object
			constexpr static size_t ssize(void) {return Curve::Xsize(dataType);};
			/// construct from a std::vector<uint8_t>
			X(const std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, ssize(), this->begin());}
			/// construct from a uint8_t *
			X(const uint8_t * const buffer) {std::copy_n(buffer, ssize(), this->begin());}
			/// default initialise value to 0
			X() {this->fill(0);};
			/// copy from a std::vector<uint8_t>
			void assign(const std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, ssize(), this->begin());}
	};

	/**
	 * @brief Key pair structure for key exchange algorithm
	 */
	template <typename Curve>
	class Xpair {
		private:
			X<Curve, lime::Xtype::publicKey> m_pubKey;
			X<Curve, lime::Xtype::privateKey> m_privKey;
		public:
			/// access the private key
			X<Curve, lime::Xtype::privateKey> &privateKey(void) {return m_privKey;};
			const X<Curve, lime::Xtype::privateKey> &cprivateKey(void) const {return m_privKey;};
			/// access the public key
			X<Curve, lime::Xtype::publicKey> &publicKey(void) {return m_pubKey;};
			const X<Curve, lime::Xtype::publicKey> &cpublicKey(void) const {return m_pubKey;};
			/// copy construct a key pair from public and private keys (no verification on validity of keys is performed)
			Xpair(const X<Curve, lime::Xtype::publicKey> &pub, const X<Curve, lime::Xtype::privateKey> &priv):m_pubKey(pub),m_privKey(priv) {};
			Xpair() :m_pubKey{},m_privKey{}{};
			/// == operator assert that public and private keys are the same
			bool operator==(Xpair<Curve> b) const {return (m_privKey==b.privateKey() && m_pubKey==b.publicKey());};

	};

	/****************************************************************/
	/* Key Encapsulation Data structures                            */
	/****************************************************************/
	/**
	 * @brief Base buffer definition for KEM data structure
	 *
	 * easy use of array types with correct size
	 */
	template <typename Algo, lime::Ktype dataType>
	class K : public sBuffer<static_cast<size_t>(Algo::Ksize(dataType))>{
		public :
			/// provide a static size function to be able to call the function not on an object
			constexpr static size_t ssize(void) {return Algo::Ksize(dataType);};
			/// construct from a std::vector<uint8_t>
			K(const std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, ssize(), this->begin());}
			/// construct from uint8_t *
			K(const uint8_t * const buffer) {std::copy_n(buffer, ssize(), this->begin());}
			/// default initialise value to 0
			K() {this->fill(0);};
			/// copy from a std::vector<uint8_t>
			void assign(const std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, ssize(), this->begin());}
	};

	/**
	 * @brief Key pair structure for key exchange algorithm
	 */
	template <typename Algo>
	class Kpair {
		private:
			K<Algo, lime::Ktype::publicKey> m_pubKey;
			K<Algo, lime::Ktype::privateKey> m_privKey;
		public:
			/// access the private key
			K<Algo, lime::Ktype::privateKey> &privateKey(void) {return m_privKey;};
			const K<Algo, lime::Ktype::privateKey> &cprivateKey(void) const {return m_privKey;};
			/// access the public key
			K<Algo, lime::Ktype::publicKey> &publicKey(void) {return m_pubKey;};
			const K<Algo, lime::Ktype::publicKey> &cpublicKey(void) const {return m_pubKey;};
			/// copy construct a key pair from public and private keys (no verification on validity of keys is performed)
			Kpair(const K<Algo, lime::Ktype::publicKey> &pub, const K<Algo, lime::Ktype::privateKey> &priv):m_pubKey(pub),m_privKey(priv) {};
			Kpair() :m_pubKey{},m_privKey{}{};
			/// == operator assert that public and private keys are the same
			bool operator==(Kpair<Algo> b) const {return (m_privKey==b.privateKey() && m_pubKey==b.publicKey());};

	};

	/****************************************************************/
	/* Digital Signature Algorithm data structures                  */
	/****************************************************************/
	/**
	 * @brief Base buffer definition for DSA data structure
	 *
	 * easy use of array types with correct size
	 */
	template <typename Curve, lime::DSAtype dataType>
	class DSA : public sBuffer<static_cast<size_t>(Curve::DSAsize(dataType))>{
		public :
			/// provide a static size function to be able to call the function not on an object
			constexpr static size_t ssize(void) {return Curve::DSAsize(dataType);};
			/// contruct from a std::vector<uint8_t>
			DSA(const std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, ssize(), this->begin());}
			/// default initialise value to 0
			DSA() {this->fill(0);};
			/// copy from a std::vector<uint8_t>
			void assign(const std::vector<uint8_t>::const_iterator buffer) {std::copy_n(buffer, ssize(), this->begin());}
	};

	/**
	 * @brief Key pair structure for DSA algorithm
	 */
	template <typename Curve>
	class DSApair {
		private:
			DSA<Curve, lime::DSAtype::publicKey> m_pubKey;
			DSA<Curve, lime::DSAtype::privateKey> m_privKey;
		public:
			/// access the private key
			DSA<Curve, lime::DSAtype::privateKey> &privateKey(void) {return m_privKey;};
			const DSA<Curve, lime::DSAtype::privateKey> &cprivateKey(void) const {return m_privKey;};
			/// access the public key
			DSA<Curve, lime::DSAtype::publicKey> &publicKey(void) {return m_pubKey;};
			const DSA<Curve, lime::DSAtype::publicKey> &cpublicKey(void) const {return m_pubKey;};
			/// copy construct a key pair from public and private keys (no verification on validity of keys is performed)
			DSApair(const DSA<Curve, lime::DSAtype::publicKey> &pub, const DSA<Curve, lime::DSAtype::privateKey> &priv):m_pubKey(pub),m_privKey(priv) {};
			DSApair() :m_pubKey{},m_privKey{}{};
			/// == operator assert that public and private keys are the same
			bool operator==(DSApair<Curve> b) const {return (m_privKey==b.privateKey() && m_pubKey==b.publicKey());};
	};


/*************************************************************************************************/
/********************** Crypto Algo interface ****************************************************/
/*************************************************************************************************/

/**
 * @brief Random number generator interface
 *
 * This abstract class is used to hold a RNG object
 * It provides explicit functions to generate:
 * - random key for Double Ratchet
 * - random keys Id on 31 bits.
 * It also expose a generic function to generate random in a buffer
 */
class RNG {
	public:
		/**
		 * @brief Generate a 32 bits unsigned integer(used to generate keys Id)
		 * The MSbit is forced to 0 to avoid dealing with DB misinterpreting unsigned values into signed one
		 * Our random number is actually on 31 bits.
		 *
		 * @return a random 32 bits unsigned integer
		 */
		virtual uint32_t randomize() = 0;

		/**
		 * fill a buffer with random numbers
		 * @param[in,out]	buffer 	The buffer to be filled with random (callers responsability to allocate memory)
		 * @param[in]		size	size in bytes of the random generated, buffer must be at least of this size
		 **/
		virtual void randomize(uint8_t *buffer, const size_t size) = 0;

		virtual ~RNG() = default;
}; //class RNG

/**
 * @brief Key exchange interface
 *
 * shall be able to provide an interface to any algorithm implementing key exchange
 * @note wrapped in algorithms must support key format convertion function from matching Digital Signature algorithm
 */
template <typename Curve>
class keyExchange {
	public:
		/* accessors */
		/// get Secret key
		virtual const X<Curve, lime::Xtype::privateKey> get_secret(void) = 0;
		/// get Self Public key
		virtual const X<Curve, lime::Xtype::publicKey> get_selfPublic(void) = 0;
		/// get Peer Public key
		virtual const X<Curve, lime::Xtype::publicKey> get_peerPublic(void) = 0;
		/// get shared secret when exchange is completed
		virtual const X<Curve, lime::Xtype::sharedSecret> get_sharedSecret(void) = 0;

		/* set keys in context, publics and private keys directly accept Signature formatted keys which are converted to keyExchange format */
		/// set Secret key
		virtual void set_secret(const X<Curve, lime::Xtype::privateKey> &secret) = 0;
		/** @overload
		 *
		 * give a DSA formatted key, this set function will convert its format to the key exchange one
		 */
		virtual void set_secret(const DSA<Curve, lime::DSAtype::privateKey> &secret) = 0;
		/// set Self Public key
		virtual void set_selfPublic(const X<Curve, lime::Xtype::publicKey> &selfPublic) = 0; /** Self Public key */
		/** @overload
		 *
		 * give a DSA formatted key, this set function will convert its format to the key exchange one
		 */
		virtual void set_selfPublic(const DSA<Curve, lime::DSAtype::publicKey> &selfPublic) = 0;
		/// set Peer Public key
		virtual void set_peerPublic(const X<Curve, lime::Xtype::publicKey> &peerPublic) = 0; /** Peer Public key */
		/** @overload
		 *
		 * give a DSA formatted key, this set function will convert its format to the key exchange one
		 */
		virtual void set_peerPublic(const DSA<Curve, lime::DSAtype::publicKey> &peerPublic) = 0;

		/**
		 * @brief generate a new random key pair
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

/**
 * @brief Key Encapsulation Mechanism interface
 *
 * shall be able to provide an interface to any algorithm implementing KEM (with PQ or not)
 */
template <typename Algo>
class KEM {
	public:
		/**
		 * @brief generate a new random key pair
		 *
		 * @param[out]	keyPair	The key pair generated
		 */
		virtual void createKeyPair(Kpair<Algo>&keyPair) = 0;

		/**
		 * @brief Generate and encapsulate a shared secret for a given public key
		 *
		 * @param[in] publicKey 	The public to encapsulate for
		 * @param[out] cipherText	The cipher text generated
		 * @param[out] sharedSecret	The shared secret generated
		 */
		virtual void encaps(const K<Algo, lime::Ktype::publicKey> &publicKey, K<Algo, lime::Ktype::cipherText> &cipherText, K<Algo, lime::Ktype::sharedSecret> &sharedSecret) = 0;
	
		/**
		 * @brief decapsulate a shared secret from a cipher text using a private key
		 *
		 * @param[in] privateKey 	The private key used to decapsulate
		 * @param[in] cipherText	The cipher text 
		 * @param[out] sharedSecret	The retrieved shared secret
		 */
		virtual void decaps(const K<Algo, lime::Ktype::privateKey> &privateKey, const K<Algo, lime::Ktype::cipherText> &cipherText, K<Algo, lime::Ktype::sharedSecret> &sharedSecret) = 0;


		virtual ~KEM() = default;
}; //class keyExchange

/**
 * @brief Digital Signature interface
 *
 * shall be able to provide an interface to any algorithm implementing digital signature
 */
template <typename Curve>
class Signature {
	public:
		/* accessors */
		/// Secret key
		virtual const DSA<Curve, lime::DSAtype::privateKey> get_secret(void) = 0;
		/// Public key
		virtual const DSA<Curve, lime::DSAtype::publicKey> get_public(void) = 0;

		/// Secret key
		virtual void set_secret(const DSA<Curve, lime::DSAtype::privateKey> &secretKey) = 0;
		/// Public key
		virtual void set_public(const DSA<Curve, lime::DSAtype::publicKey> &publicKey) = 0;

		/**
		 * @brief generate a new random EdDSA key pair
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
		virtual void sign(const std::vector<uint8_t> &message, DSA<Curve, lime::DSAtype::signature> &signature) = 0;
		/**
		 * @overload virtual void sign(const X<Curve, lime::Xtype::publicKey> &message, DSA<Curve, lime::DSAtype::signature> &signature)
		 * a convenience function to directly verify a key exchange public key
		 */
		virtual void sign(const X<Curve, lime::Xtype::publicKey> &message, DSA<Curve, lime::DSAtype::signature> &signature) = 0;

		/**
		 * @brief Verify a message signature using the public key previously set in the object
		 *
		 * @param[in]	message		The message signed (we can verify any vector or more specifically a key exchange public key)
		 * @param[in]	signature	The signature produced from the message with a key pair previously introduced in the object
		 *
		 * @return	true if the signature is valid, false otherwise
		 */
		virtual bool verify(const std::vector<uint8_t> &message, const DSA<Curve, lime::DSAtype::signature> &signature) = 0;
		/**
		 * @overload virtual bool verify(const X<Curve, lime::Xtype::publicKey> &message, const DSA<Curve, lime::DSAtype::signature> &signature)
		 * a convenience function to directly verify a key exchange public key
		 */
		virtual bool verify(const X<Curve, lime::Xtype::publicKey> &message, const DSA<Curve, lime::DSAtype::signature> &signature) = 0;

		virtual ~Signature() = default;
}; //class EdDSA

/**
 * @brief templated HMAC
 *
 * @tparam	hashAlgo	the hash algorithm used (only SHA512 available for now)
 *
 * @param[in]	key		HMAC key
 * @param[in]	keySize		previous buffer size
 * @param[in]	input		HMAC input
 * @param[in]	inputSize	previous buffer size
 * @param[out]	hash		pointer to the output, this buffer must be able to hold as much data as requested
 * @param[in]	hashSize	amount of expected data, if more than selected Hash algorithm can compute, silently ignored and maximum output size is generated
 *
 */
template <typename hashAlgo>
void HMAC(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize);
/* declare template specialisations */
template <> void HMAC<SHA512>(const uint8_t *const key, const size_t keySize, const uint8_t *const input, const size_t inputSize, uint8_t *hash, size_t hashSize);

/**
 * @brief HKDF as described in RFC5869
 *	@par Compute:
 *	@code{.unparsed}
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
 *	@endcode
 *
 * @tparam	hashAlgo	the hash algorithm to use (SHA512 available for now)
 *
 * @param[in]	salt 		salt
 * @param[in]	saltSize	saltSize
 * @param[in]	ikm		input key material
 * @param[in]	ikmSize		input key material size
 * @param[in]	info		a info string or buffer
 * @param[in]	infoSize	the info buffer size
 * @param[out]	okm		output key material
 * @param[in]	okmSize		requested amount of data, okm buffer must be able to hold it. (L in the RFC doc)
 *
 */
template <typename hashAlgo>
void HMAC_KDF(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const char *info, const size_t infoSize, uint8_t *output, size_t outputSize);
/* declare template specialisations */
template <> void HMAC_KDF<SHA512>(const uint8_t *const salt, const size_t saltSize, const uint8_t *const ikm, const size_t ikmSize, const char * info, const size_t infoSize, uint8_t *output, size_t outputSize);


/************************ AEAD interface *************************************/

/**
 * @brief Encrypt and tag using scheme given as template parameter
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
 * @brief Authenticate and Decrypt using scheme given as template parameter
 *
 * @param[in]	key		Encryption key
 * @param[in]	keySize		Key buffer length, it must match the selected AEAD scheme or an exception is generated
 * @param[in]	IV		Buffer holding the initialisation vector
 * @param[in]	IVSize		Initialisation vector length in bytes
 * @param[in]	cipher		Buffer holding the data to be decrypted
 * @param[in]	cipherSize	Length in bytes of buffer to be decrypted
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
/* Use these to instantiate an object as they will pick the correct underlying implemenation of virtual classes */
std::shared_ptr<RNG> make_RNG();

template <typename Curve>
std::shared_ptr<keyExchange<Curve>> make_keyExchange();

template <typename Curve>
std::shared_ptr<Signature<Curve>> make_Signature();

template <typename Algo>
std::shared_ptr<KEM<Algo>> make_KEM();

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

#ifdef HAVE_BCTBXPQ
	extern template std::shared_ptr<KEM<K512>> make_KEM();
	extern template class K<K512, lime::Ktype::publicKey>;
	extern template class K<K512, lime::Ktype::privateKey>;
	extern template class K<K512, lime::Ktype::cipherText>;
	extern template class K<K512, lime::Ktype::sharedSecret>;
	extern template class Kpair<K512>;
#endif //HAVE_BCTBXPQ
} // namespace lime

#endif //lime_crypto_primitives_hpp


