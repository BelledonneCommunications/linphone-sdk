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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <mbedtls/base64.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>
#include <psa/crypto.h>

#include "bctoolbox/crypto.hh"
#include "bctoolbox/exception.hh"
#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"

#include <array>
#include <memory>

namespace bctoolbox {

namespace {
// This is also defined in mbedtls source code by a custom modification
using mbedtls_threading_mutex_t = void *;

void threading_mutex_init_cpp(mbedtls_threading_mutex_t *mutex) {
	if (mutex == NULL) {
		return;
	}
	auto m = new std::mutex();
	*mutex = (void *)m;
}

void threading_mutex_free_cpp(mbedtls_threading_mutex_t *mutex) {
	if (mutex == NULL) {
		return;
	}
	delete (static_cast<std::mutex *>(*mutex));
}

int threading_mutex_lock_cpp(mbedtls_threading_mutex_t *mutex) {
	if (mutex == NULL) {
		return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
	}
	static_cast<std::mutex *>(*mutex)->lock();
	return 0;
}

int threading_mutex_unlock_cpp(mbedtls_threading_mutex_t *mutex) {
	if (mutex == NULL) {
		return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
	}
	static_cast<std::mutex *>(*mutex)->unlock();
	return 0;
}

class mbedtlsStaticContexts {
public:
	void randomize(uint8_t *buffer, size_t size) {
		sRNG->randomize(buffer, size);
	}

	std::unique_ptr<RNG> sRNG;
	mbedtlsStaticContexts() {
		mbedtls_threading_set_alt(threading_mutex_init_cpp, threading_mutex_free_cpp, threading_mutex_lock_cpp,
		                          threading_mutex_unlock_cpp);
		if (psa_crypto_init() != PSA_SUCCESS) {
			bctbx_error("MbedTLS PSA init fail");
		}
		// Now that mbedtls is ready, instanciate the static RNG
		sRNG = std::make_unique<RNG>();
	}
	~mbedtlsStaticContexts() {
		// before destroying mbedtls internal context, destroy the static RNG
		sRNG = nullptr;
		mbedtls_psa_crypto_free();
		mbedtls_threading_free_alt();
	}
};
static const auto mbedtlsStaticContextsInstance = std::make_unique<mbedtlsStaticContexts>();
}; // namespace

/*****************************************************************************/
/***                      Random Number Generation                         ***/
/*****************************************************************************/
/**
 * @brief Wrapper around mbedtls implementation
 **/
struct RNG::Impl {

	mbedtls_entropy_context entropy;   /**< entropy context - store it to be able to free it */
	mbedtls_ctr_drbg_context ctr_drbg; /**< rng context */

	/**
	 * Implementation constructor
	 * Initialise the entropy and RNG context
	 */
	Impl() {
		mbedtls_entropy_init(&entropy);
		mbedtls_ctr_drbg_init(&ctr_drbg);
		if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0) != 0) {
			throw BCTBX_EXCEPTION << "RNG failure at creation: entropy source failure";
		}
	}
	~Impl() {
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
	}
};

/**
 * Constructor
 * Just instanciate an implementation
 */
RNG::RNG() : pImpl(std::make_unique<RNG::Impl>()) {};

/**
 * Destructor
 * Implementation is stored in a unique_ptr, nothing to do
 **/
RNG::~RNG() = default;

void RNG::randomize(uint8_t *buffer, size_t size) {
	int ret = mbedtls_ctr_drbg_random(&(pImpl->ctr_drbg), buffer, size);
	if (ret != 0) {
		throw BCTBX_EXCEPTION << ((ret == MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG)
		                              ? "RNG failure: Request too big"
		                              : "RNG failure: entropy source failure");
	}
}

std::vector<uint8_t> RNG::randomize(const size_t size) {
	std::vector<uint8_t> buffer(size);
	int ret = mbedtls_ctr_drbg_random(&(pImpl->ctr_drbg), buffer.data(), size);
	if (ret != 0) {
		throw BCTBX_EXCEPTION << ((ret == MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG)
		                              ? "RNG failure: Request too big"
		                              : "RNG failure: entropy source failure");
	}
	return buffer;
}

uint32_t RNG::randomize() {
	uint8_t buffer[4];
	randomize(buffer, 4);
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/*
 * class randomize functions
 * These use the class RNG context
 */
void RNG::cRandomize(uint8_t *buffer, size_t size) {
	mbedtlsStaticContextsInstance->randomize(buffer, size);
}

uint32_t RNG::cRandomize() {
	uint8_t buffer[4];
	cRandomize(buffer, 4);
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/*****************************************************************************/
/***                      Hash related function                            ***/
/*****************************************************************************/
/* HKDF specialized template for SHA256 */
template <>
std::vector<uint8_t> HKDF<SHA256>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::vector<uint8_t> &info,
                                  size_t outputSize) {
	std::vector<uint8_t> okm(outputSize);
	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt.data(), salt.size(), ikm.data(), ikm.size(),
	                 info.data(), info.size(), okm.data(), outputSize) != 0) {
		throw BCTBX_EXCEPTION << "HKDF-SHA256 error";
	}
	return okm;
};
template <>
std::vector<uint8_t> HKDF<SHA256>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::string &info,
                                  size_t outputSize) {
	std::vector<uint8_t> okm(outputSize);
	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), salt.data(), salt.size(), ikm.data(), ikm.size(),
	                 reinterpret_cast<const unsigned char *>(info.data()), info.size(), okm.data(), outputSize) != 0) {
		throw BCTBX_EXCEPTION << "HKDF-SHA256 error";
	}
	return okm;
};

/* HKDF specialized template for SHA384 */
template <>
std::vector<uint8_t> HKDF<SHA384>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::vector<uint8_t> &info,
                                  size_t outputSize) {
	std::vector<uint8_t> okm(outputSize);
	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA384), salt.data(), salt.size(), ikm.data(), ikm.size(),
	                 info.data(), info.size(), okm.data(), outputSize) != 0) {
		throw BCTBX_EXCEPTION << "HKDF-SHA384 error";
	}
	return okm;
};
template <>
std::vector<uint8_t> HKDF<SHA384>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::string &info,
                                  size_t outputSize) {
	std::vector<uint8_t> okm(outputSize);
	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA384), salt.data(), salt.size(), ikm.data(), ikm.size(),
	                 reinterpret_cast<const unsigned char *>(info.data()), info.size(), okm.data(), outputSize) != 0) {
		throw BCTBX_EXCEPTION << "HKDF-SHA384 error";
	}
	return okm;
};

/* HKDF specialized template for SHA512 */
template <>
std::vector<uint8_t> HKDF<SHA512>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::vector<uint8_t> &info,
                                  size_t outputSize) {
	std::vector<uint8_t> okm(outputSize);
	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), salt.data(), salt.size(), ikm.data(), ikm.size(),
	                 info.data(), info.size(), okm.data(), outputSize) != 0) {
		throw BCTBX_EXCEPTION << "HKDF-SHA512 error";
	}
	return okm;
};
template <>
std::vector<uint8_t> HKDF<SHA512>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::string &info,
                                  size_t outputSize) {
	std::vector<uint8_t> okm(outputSize);
	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), salt.data(), salt.size(), ikm.data(), ikm.size(),
	                 reinterpret_cast<const unsigned char *>(info.data()), info.size(), okm.data(), outputSize) != 0) {
		throw BCTBX_EXCEPTION << "HKDF-SHA512 error";
	}
	return okm;
};
template <>
void HKDF<SHA512>(const uint8_t *salt,
                  const size_t saltSize,
                  const uint8_t *ikm,
                  const size_t ikmSize,
                  const char *info,
                  const size_t infoSize,
                  uint8_t *okm,
                  size_t okmSize) {
	if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), salt, saltSize, ikm, ikmSize,
	                 reinterpret_cast<const unsigned char *>(info), infoSize, okm, okmSize) != 0) {
		throw BCTBX_EXCEPTION << "HKDF-SHA512 error";
	}
};

/*****************************************************************************/
/***                      Key Wrap                                         ***/
/*****************************************************************************/

int AES_key_wrap(const std::vector<uint8_t> &plaintext,
                 const std::vector<uint8_t> &key,
                 std::vector<uint8_t> &ciphertext,
                 AesId id) {

	uint64_t t;
	size_t m = plaintext.size();                                     // Size of the plaintext
	size_t r;                                                        // Size of the padded plaintext
	size_t n;                                                        // Number of 64-bit blocks in the padded plaintext
	uint8_t input[16];                                               // Buffer of the AES input
	uint8_t B[16];                                                   // Buffer of the AES output
	uint8_t *R = (uint8_t *)bctbx_malloc((m + 8) * sizeof(uint8_t)); // An array of 8-bit registers

	/* Append padding */
	r = m;
	memcpy(R, plaintext.data(), r);
	while (r % 8 != 0) {
		R[r] = 0;
		r++;
	}
	n = r / 8;

	/* Initialise variables */
	// Initialisation of A described in the RFC 5649 Section 3 : https://datatracker.ietf.org/doc/html/rfc5649#section-3
	uint8_t A[8];
	A[0] = 0xA6;
	A[1] = 0x59;
	A[2] = 0x59;
	A[3] = 0xA6;
	for (size_t i = 0; i < 4; i++) {
		A[4 + i] = (m >> (3 - i) * 8) & 0xFF;
	}

	// Initialise AES context with the key
	mbedtls_aes_context context;
	mbedtls_aes_init(&context);
	switch (id) {
		case AesId::AES128:
			mbedtls_aes_setkey_enc(&context, key.data(), 128);
			break;
		case AesId::AES192:
			mbedtls_aes_setkey_enc(&context, key.data(), 192);
			break;
		case AesId::AES256:
			mbedtls_aes_setkey_enc(&context, key.data(), 256);
			break;
		default:
			return BCTBX_ERROR_INVALID_INPUT_DATA;
	}

	/* Calculate intermediate values */
	if (r == 8) {
		// input = concat(A, R[0])
		memcpy(input, A, 8);
		memcpy(input + 8, R, 8);

		// B = AES(key, input)
		mbedtls_aes_crypt_ecb(&context, MBEDTLS_AES_ENCRYPT, input, B);

		/* Output the results */
		ciphertext.assign(B, B + 16);

		mbedtls_aes_free(&context);
		bctbx_free(R);

		return 0;
	}
	for (int j = 0; j <= 5; j++) {
		for (size_t i = 0; i < n; i++) {
			// input = concat(A, R[i])
			memcpy(input, A, 8);
			memcpy(input + 8, R + (i * 8), 8);

			// B = AES(key, input)
			mbedtls_aes_crypt_ecb(&context, MBEDTLS_AES_ENCRYPT, input, B);

			// A = MSB(64, B) ^ t where t = (n*j)+i
			t = (n * j) + i + 1;
			for (size_t k = 0; k < 8; k++) {
				A[k] = B[k] ^ ((t >> (7 - k) * 8) & 0xFF);
			}

			// R[i] = LSB(64, B)
			memcpy(R + (i * 8), B + 8, 8);
		}
	}

	/* Output the results */
	ciphertext.assign(A, A + 8);
	ciphertext.insert(ciphertext.end(), R, R + r);

	mbedtls_aes_free(&context);
	bctbx_free(R);

	return 0;
}

int AES_key_unwrap(const std::vector<uint8_t> &ciphertext,
                   const std::vector<uint8_t> &key,
                   std::vector<uint8_t> &plaintext,
                   AesId id) {

	size_t n = (ciphertext.size() - 8) / 8;                    // Number of 64-bit blocks of the padded plaintext
	size_t r = ciphertext.size() - 8;                          // Size of the padded plaintext
	size_t m = 0;                                              // Size of the plaintext
	uint8_t input[16];                                         // Buffer of the AES input
	uint8_t B[16];                                             // Buffer of the AES output
	uint8_t *R = (uint8_t *)bctbx_malloc(r * sizeof(uint8_t)); // An array of 8-bit registers

	/* Initialise variables */
	uint8_t A[8];
	memcpy(A, ciphertext.data(), 8);
	memcpy(R, ciphertext.data() + 8, r);

	// Initialise AES context with the key
	mbedtls_aes_context context;
	mbedtls_aes_init(&context);
	switch (id) {
		case AesId::AES128:
			mbedtls_aes_setkey_dec(&context, key.data(), 128);
			break;
		case AesId::AES192:
			mbedtls_aes_setkey_dec(&context, key.data(), 192);
			break;
		case AesId::AES256:
			mbedtls_aes_setkey_dec(&context, key.data(), 256);
			break;
		default:
			mbedtls_aes_free(&context);
			bctbx_free(R);
			return BCTBX_ERROR_INVALID_INPUT_DATA;
	}

	/* Compute intermediate values */
	if (n == 1) {
		// input = ciphertext
		memcpy(input, ciphertext.data(), 16);

		// B = AES-1(K, input)
		mbedtls_aes_crypt_ecb(&context, MBEDTLS_AES_DECRYPT, input, B);

		// A = MSB(64, B)
		memcpy(A, B, 8);

		// R[i] = LSB(64, B)
		memcpy(R, B + 8, 8);
	} else {
		for (int j = 5; j >= 0; j--) {
			for (size_t i = n; i > 0; i--) {
				// input = concat(A ^ t, R[i]) where t = n*j+i
				uint64_t t = (n * j) + i;
				for (size_t k = 0; k < 8; k++) {
					input[k] = A[k] ^ ((t >> (7 - k) * 8) & 0xFF);
					input[k + 8] = R[(i - 1) * 8 + k];
				}

				// B = AES-1(K, input)
				mbedtls_aes_crypt_ecb(&context, MBEDTLS_AES_DECRYPT, input, B);

				// A = MSB(64, B)
				memcpy(A, B, 8);

				// R[i] = LSB(64, B)
				memcpy(R + ((i - 1) * 8), B + 8, 8);
			}
		}
	}

	/* Output the results */
	mbedtls_aes_free(&context);
	// AIV verification described in the RFC 5649 Section 3 : https://datatracker.ietf.org/doc/html/rfc5649#section-3
	if (!(A[0] == 0xA6 && A[1] == 0x59 && A[2] == 0x59 && A[3] == 0xA6)) {
		bctbx_free(R);
		return BCTBX_ERROR_UNSPECIFIED_ERROR;
	}
	// m = concat(A[4], A[5], A[6], A[7])
	for (size_t i = 0; i < 4; i++) {
		m |= A[4 + i] << ((3 - i) * 8);
	}
	if ((m <= 8 * (n - 1)) || (8 * n <= m)) {
		bctbx_free(R);
		return BCTBX_ERROR_UNSPECIFIED_ERROR;
	}

	// Remove padding & Return plaintext
	plaintext.assign(R, R + m);

	bctbx_free(R);

	return 0;
}

std::string encodeBase64(const std::vector<uint8_t> &input) {
	size_t byteWritten = 0;
	size_t outputLength = 0;
	auto inputBuffer = input.data();
	size_t inputLength = input.size();
	mbedtls_base64_encode(nullptr, outputLength, &byteWritten, inputBuffer,
	                      inputLength); // set encodedLength to the correct value
	outputLength = byteWritten;
	if (outputLength == 0) return std::string();
	unsigned char *outputBuffer = new unsigned char[outputLength]; // allocate encoded buffer with correct length
	int ret =
	    mbedtls_base64_encode(outputBuffer, outputLength, &byteWritten, inputBuffer, inputLength); // real encoding
	if (ret != 0) {
		delete[] outputBuffer;
		return std::string();
	}
	std::string output((char *)outputBuffer);
	delete[] outputBuffer;
	return output;
}

std::vector<uint8_t> decodeBase64(const std::string &input) {

	size_t missingPaddingSize = 0;
	std::string paddedInput{};
	// mbedtls function does not work well if the padding is omitted, restore it if needed
	if ((input.size() % 4) != 0) {
		missingPaddingSize = 4 - (input.size() % 4);
		paddedInput = input;
		if (missingPaddingSize == 1) {
			paddedInput.append("=");
		}
		if (missingPaddingSize == 2) {
			paddedInput.append("==");
		}
	}
	size_t byteWritten = 0;
	size_t outputLength = 0;
	const unsigned char *inputBuffer =
	    (missingPaddingSize == 0) ? (const unsigned char *)input.data() : (const unsigned char *)paddedInput.data();
	size_t inputLength = input.size() + missingPaddingSize;
	mbedtls_base64_decode(nullptr, outputLength, &byteWritten, inputBuffer,
	                      inputLength); // set decodedLength to the correct value
	outputLength = byteWritten;
	unsigned char *outputBuffer = new unsigned char[outputLength]; // allocate decoded buffer with correct length
	int ret =
	    mbedtls_base64_decode(outputBuffer, outputLength, &byteWritten, inputBuffer, inputLength); // real decoding
	if (ret != 0) {
		delete[] outputBuffer;
		return std::vector<uint8_t>();
	}
	std::vector<uint8_t> output(outputBuffer, outputBuffer + outputLength);
	delete[] outputBuffer;
	return output;
}

} // namespace bctoolbox
