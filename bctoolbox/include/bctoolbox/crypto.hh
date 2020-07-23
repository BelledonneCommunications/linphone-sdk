/*
 * Copyright (c) 2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
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

#ifndef BCTBX_CRYPTO_HH
#define BCTBX_CRYPTO_HH

#include <vector>
#include <memory>

namespace bctoolbox {
/**
 * @brief Random number generator interface
 *
 * This wrapper provides an interface to a RNG.
 * Two ways to get some random numbers:
 *  - calling the static class functions(cRandomize) : do not use this to feed cryptographic functions
 *  - instanciate a RNG object and call the randomize method : use this one for cryptographic quality random
 *
 * Any call (including creation), may throw an exception if some error are detected on the random source
 */
class RNG {
	public:
		/**
		 * fill a buffer with random numbers
		 * @param[in,out]	buffer 	The buffer to be filled with random (callers responsability to allocate memory)
		 * @param[in]		size	size in bytes of the random generated, buffer must be at least of this size
		 **/
		void randomize(uint8_t *buffer, const size_t size);

		/**
		 * return a random vector of given size
		 * @param[in]		size	size in bytes of the random generated
		 **/
		std::vector<uint8_t> randomize(const size_t size);


		/**
		 * generates a 32 bits random unsigned number
		 **/
		uint32_t randomize();

		/**
		 * fill a buffer with random numbers
		 * @param[in,out]	buffer 	The buffer to be filled with random (callers responsability to allocate memory)
		 * @param[in]		size	size in bytes of the random generated, buffer must be at least of this size
		 *
		 * @note This function uses a shared RNG context, do not use it to generate sensitive material
		 **/
		static void cRandomize(uint8_t *buffer, size_t size);
		/**
		 * generates a 32 bits random unsigned number
		 *
		 * @note This function uses a shared RNG context, do not use it to generate sensitive material
		 **/
		static uint32_t cRandomize();

		RNG();
		~RNG();
	private:
		struct Impl;
		std::unique_ptr<Impl> pImpl;
		static std::unique_ptr<Impl> pImplClass;
}; //class RNG


/*****************************************************************************/
/***                      Hash related function                            ***/
/*****************************************************************************/
/**
 * @brief SHA256 buffer size definition
 */
struct SHA256 {
	/// maximum output size for SHA256 is 32 bytes
	static constexpr size_t ssize() {return 32;}
};

/**
 * @brief SHA384 buffer size definition
 */
struct SHA384 {
	/// maximum output size for SHA384 is 48 bytes
	static constexpr size_t ssize() {return 48;}
};

/**
 * @brief SHA512 buffer size definition
 */
struct SHA512 {
	/// maximum output size for SHA512 is 64 bytes
	static constexpr size_t ssize() {return 64;}
};


/**
 * @brief templated HMAC
 *
 * @tparam	hashAlgo	the hash algorithm used: SHA256, SHA384, SHA512
 *
 * @param[in]	key		HMAC key
 * @param[in]	input		HMAC input
 *
 * @return an array of size matching the selected hash algorithm output size
 *
 */
template <typename hashAlgo>
std::vector<uint8_t> HMAC(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input);
/* declare template specialisations */
template <> std::vector<uint8_t>  HMAC<SHA256>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input);
template <> std::vector<uint8_t>  HMAC<SHA384>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input);
template <> std::vector<uint8_t>  HMAC<SHA512>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input);

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
 * @tparam	hashAlgo	the hash algorithm to use
 * @tparam	infoType	the info parameter type : can be passed as a string or a std::vector<uint8_t>
 *
 * @param[in]	salt 		salt
 * @param[in]	ikm		input key material
 * @param[in]	info		a info string or buffer
 * @param[in]	okmSize		requested amount of data. (L in the RFC doc)
 *
 * @return the output key material
 *
 */
template <typename hashAlgo>
std::vector<uint8_t> HKDF(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::vector<uint8_t> &info, size_t okmSize);
template <typename hashAlgo>
std::vector<uint8_t> HKDF(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::string &info, size_t okmSize);
/* declare template specialisations */
template <> std::vector<uint8_t> HKDF<SHA256>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::vector<uint8_t> &info, size_t outputSize);
template <> std::vector<uint8_t> HKDF<SHA256>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::string &info, size_t outputSize);
template <> std::vector<uint8_t> HKDF<SHA512>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::vector<uint8_t> &info, size_t outputSize);
template <> std::vector<uint8_t> HKDF<SHA512>(const std::vector<uint8_t> &salt, const std::vector<uint8_t> &ikm, const std::string &info, size_t outputSize);


/************************ AEAD interface *************************************/
// AEAD function defines
/**
 * @brief AES256GCM buffers size definition
 */
struct AES256GCM128 {
	/// key size is 32 bytes
	static constexpr size_t keySize(void) {return 32;};
	/// tag size is 16 bytes
	static constexpr size_t tagSize(void) {return 16;};
};

/**
 * @brief Encrypt and tag using scheme given as template parameter
 *
 * @param[in]	key		Encryption key
 * @param[in]	IV		Initialisation vector
 * @param[in]	plain		Plain text
 * @param[in]	AD		Additional data used in tag computation
 * @param[out]	tag		Generated authentication tag
 * @return	the cipher text
 */
template <typename AEADAlgo>
std::vector<uint8_t> AEADEncrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> IV, const std::vector<uint8_t> &plain, const std::vector<uint8_t> &AD,
		std::vector<uint8_t> &tag);

/**
 * @brief Authenticate and Decrypt using scheme given as template parameter
 *
 * @param[in]	key		Encryption key
 * @param[in]	IV		Initialisation vector
 * @param[in]	cipher		Cipher text
 * @param[in]	AD		Additional data used in tag computation
 * @param[in]	tag		Authentication tag
 * @param[out]	plain		A vector to store the plain text
 *
 * @return true if authentication tag match and decryption was successful
 */
template <typename AEADAlgo>
bool AEADDecrypt(const std::vector<uint8_t> &key, const std::vector<uint8_t> &IV, const std::vector<uint8_t> &cipher, const std::vector<uint8_t> &AD,
		const std::vector<uint8_t> &tag, std::vector<uint8_t> &plain);

/* declare AEAD template specialisations : AES256-GCM with 128 bits auth tag*/
template <> std::vector<uint8_t> AEADEncrypt<AES256GCM128>(const std::vector<uint8_t> &key, const std::vector<uint8_t> IV, const std::vector<uint8_t> &plain, const std::vector<uint8_t> &AD,
		std::vector<uint8_t> &tag);

template <> bool AEADDecrypt<AES256GCM128>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &IV, const std::vector<uint8_t> &cipher, const std::vector<uint8_t> &AD,
		const std::vector<uint8_t> &tag, std::vector<uint8_t> &plain);

} // namespace bctoolbox
#endif // BCTBX_CRYPTO_HH


