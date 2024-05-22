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

#include "bctoolbox/crypto.h"
#include "bctoolbox/crypto.hh"
#include "bctoolbox/exception.hh"
#include "bctoolbox/logging.h"

#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/rand.h"
#include <openssl/core_names.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <array>

namespace bctoolbox {

/*****************************************************************************/
/***                      Random Number Generation                         ***/
/*****************************************************************************/

struct RNG::Impl {
	Impl() {
	}

	~Impl() {
	}
};

RNG::RNG() : pImpl() {
}

RNG::~RNG() = default;

void RNG::randomize(uint8_t *buffer, size_t size) {
	int ret = RAND_priv_bytes(buffer, size);
	unsigned long err = ERR_get_error();
	if (ret == 1) {
		return;
	} else if (ret == -1) {
		throw BCTBX_EXCEPTION << ("RAND_priv_bytes(): not supported by the current RAND method");
	} else {
		std::string errString = ERR_error_string(err, NULL);
		throw BCTBX_EXCEPTION << ("RAND_priv_bytes(): [" + std::to_string(err) + "] " + errString);
	}
}

std::vector<uint8_t> RNG::randomize(const size_t size) {
	std::vector<uint8_t> buffer(size);
	randomize(buffer.data(), buffer.size());
	return buffer;
}

uint32_t RNG::randomize() {
	std::vector<uint8_t> buffer(4);
	randomize(buffer.data(), buffer.size());
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/*
 * class randomize functions
 * These use the public openssl context
 */
void RNG::cRandomize(uint8_t *buffer, size_t size) {
	int ret = RAND_bytes(buffer, size);
	unsigned long err = ERR_get_error();
	if (ret == 1) {
		return;
	} else if (ret == -1) {
		throw BCTBX_EXCEPTION << ("RAND_bytes(): not supported by the current RAND method");
	} else {
		std::string errString = ERR_error_string(err, NULL);
		throw BCTBX_EXCEPTION << ("RAND_bytes(): [" + std::to_string(err) + "] " + errString);
	}
}

uint32_t RNG::cRandomize() {
	uint8_t buffer[4];
	cRandomize(buffer, 4);
	return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
}

/*****************************************************************************/
/***                      Hash related function                            ***/
/*****************************************************************************/
void HMAC_KDF(const std::string &hashAlgorithm,
              const uint8_t *salt,
              const size_t saltSize,
              const uint8_t *ikm,
              const size_t ikmSize,
              const char *info,
              const size_t infoSize,
              uint8_t *output,
              size_t outputSize) {
	auto kdf = std::shared_ptr<EVP_KDF>(EVP_KDF_fetch(NULL, "HKDF", NULL), EVP_KDF_free);

	if (kdf == nullptr) {
		bctbx_error("EVP_KDF_fetch failed.");
		throw BCTBX_EXCEPTION << "HKDF-SHA512 error";
	}

	auto kctx = std::shared_ptr<EVP_KDF_CTX>(EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free);

	OSSL_PARAM params[] = {
	    OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, (char *)hashAlgorithm.data(), hashAlgorithm.size()),
	    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void *)ikm, ikmSize),
	    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void *)info, infoSize),
	    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)salt, saltSize), OSSL_PARAM_END};

	if (EVP_KDF_derive(kctx.get(), output, outputSize, params) <= 0) {
		bctbx_error("EVP_KDF_derive failed.");
		throw BCTBX_EXCEPTION << "HKDF-SHA512 error";
	}
}

template <typename infoType>
std::vector<uint8_t> HMAC_KDF(const std::string &hashAlgorithm,
                              const std::vector<uint8_t> &salt,
                              const std::vector<uint8_t> &ikm,
                              const infoType &info,
                              size_t outputSize) {
	std::vector<uint8_t> out(outputSize);

	auto kdf = std::shared_ptr<EVP_KDF>(EVP_KDF_fetch(NULL, "HKDF", NULL), EVP_KDF_free);

	if (kdf == nullptr) {
		out.clear();
		bctbx_error("EVP_KDF_fetch failed.");
		return out;
	}

	auto kctx = std::shared_ptr<EVP_KDF_CTX>(EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free);

	OSSL_PARAM params[] = {
	    OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, (char *)hashAlgorithm.data(), hashAlgorithm.size()),
	    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void *)ikm.data(), ikm.size()),
	    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void *)info.data(), info.size()),
	    OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, (void *)salt.data(), salt.size()), OSSL_PARAM_END};

	if (EVP_KDF_derive(kctx.get(), out.data(), out.size(), params) <= 0) {
		out.clear();
		bctbx_error("EVP_KDF_derive failed.");
	}

	return out;
}

/* HKDF specialized template for SHA256 */
template <>
std::vector<uint8_t> HKDF<SHA256>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::vector<uint8_t> &info,
                                  size_t outputSize) {
	return HMAC_KDF<std::vector<uint8_t>>(SN_sha256, salt, ikm, info, outputSize);
};

template <>
std::vector<uint8_t> HKDF<SHA256>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::string &info,
                                  size_t outputSize) {
	return HMAC_KDF<std::string>(SN_sha256, salt, ikm, info, outputSize);
};

/* HKDF specialized template for SHA384 */
template <>
std::vector<uint8_t> HKDF<SHA384>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::vector<uint8_t> &info,
                                  size_t outputSize) {
	return HMAC_KDF<std::vector<uint8_t>>(SN_sha384, salt, ikm, info, outputSize);
};

template <>
std::vector<uint8_t> HKDF<SHA384>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::string &info,
                                  size_t outputSize) {
	return HMAC_KDF<std::string>(SN_sha384, salt, ikm, info, outputSize);
};

/* HKDF specialized template for SHA512 */
template <>
std::vector<uint8_t> HKDF<SHA512>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::vector<uint8_t> &info,
                                  size_t outputSize) {
	return HMAC_KDF<std::vector<uint8_t>>(SN_sha512, salt, ikm, info, outputSize);
};

template <>
std::vector<uint8_t> HKDF<SHA512>(const std::vector<uint8_t> &salt,
                                  const std::vector<uint8_t> &ikm,
                                  const std::string &info,
                                  size_t outputSize) {
	return HMAC_KDF<std::string>(SN_sha512, salt, ikm, info, outputSize);
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
	HMAC_KDF(SN_sha512, salt, saltSize, ikm, ikmSize, info, infoSize, okm, okmSize);
};

/*****************************************************************************/
/***                      Key Wrap                                         ***/
/*****************************************************************************/
static const EVP_CIPHER *getAesKeyWrapCipher(AesId id) {
	switch (id) {
		case AesId::AES128:
			return EVP_aes_128_wrap_pad();
		case AesId::AES192:
			return EVP_aes_192_wrap_pad();
		case AesId::AES256:
			return EVP_aes_256_wrap_pad();
		default:
			return nullptr;
	}
}

static int evp_aes_key_wrap(const std::vector<uint8_t> &in,
                            const std::vector<uint8_t> &key,
                            std::vector<uint8_t> &out,
                            AesId id,
                            int encrypting) {
	const EVP_CIPHER *cipher = getAesKeyWrapCipher(id);
	if (cipher == nullptr) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == nullptr) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}

	switch (id) {
		case AesId::AES128:
			out.resize(in.size() + 16);
			break;
		case AesId::AES192:
			out.resize(in.size() + 24);
			break;
		case AesId::AES256:
			out.resize(in.size() + 32);
			break;
	}
	int update_len = 0;
	int final_len = 0;

	if (EVP_CipherInit_ex(ctx, cipher, nullptr, key.data(), nullptr, encrypting) &&
	    EVP_CipherUpdate(ctx, out.data(), &update_len, in.data(), in.size()) &&
	    EVP_CipherFinal_ex(ctx, out.data() + update_len, &final_len)) {
		out.resize(update_len + final_len);
		EVP_CIPHER_CTX_free(ctx);
		return 0;
	}
	EVP_CIPHER_CTX_free(ctx);
	return BCTBX_ERROR_UNSPECIFIED_ERROR;
}

int AES_key_wrap(const std::vector<uint8_t> &plaintext,
                 const std::vector<uint8_t> &key,
                 std::vector<uint8_t> &ciphertext,
                 AesId id) {
	return evp_aes_key_wrap(plaintext, key, ciphertext, id, 1);
}

int AES_key_unwrap(const std::vector<uint8_t> &ciphertext,
                   const std::vector<uint8_t> &key,
                   std::vector<uint8_t> &plaintext,
                   AesId id) {
	return evp_aes_key_wrap(ciphertext, key, plaintext, id, 0);
}

/*****************************************************************************/
/***                      Base64                                           ***/
/*****************************************************************************/
std::string encodeBase64(const std::vector<uint8_t> &input) {
	size_t outputLength = 0;
	std::string output{};
	int ret = bctbx_base64_encode(nullptr, &outputLength, input.data(), input.size());
	if (ret == 0 && outputLength > 0) {
		output = std::string(outputLength, '\0');
		ret = bctbx_base64_encode((unsigned char *)output.data(), &outputLength, input.data(), input.size());
	}
	if (ret != 0) {
		output.clear();
	}
	return output;
}

std::vector<uint8_t> decodeBase64(const std::string &input) {
	size_t outputLength = 0;
	std::vector<uint8_t> output{};
	int ret = bctbx_base64_decode(nullptr, &outputLength, (unsigned char *)input.data(), input.size());
	if (ret == 0 && outputLength > 0) {
		output = std::vector<uint8_t>(outputLength, '\0');
		ret = bctbx_base64_decode((unsigned char *)output.data(), &outputLength, (unsigned char *)input.data(),
		                          input.size());
		output.resize(outputLength);
	}
	if (ret != 0) {
		output.clear();
	}
	return output;
}

} // namespace bctoolbox
