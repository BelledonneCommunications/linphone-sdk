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
#include "bctoolbox/defs.h"
#include "bctoolbox/exception.hh"

#include <array>

namespace bctoolbox {

/*****************************************************************************/
/***                      Hash related function                            ***/
/*****************************************************************************/
/* HMAC templates */
/* HMAC must use a specialized template */
template <typename hashAlgo>
std::vector<uint8_t> HMAC(BCTBX_UNUSED(const std::vector<uint8_t> &key),
                          BCTBX_UNUSED(const std::vector<uint8_t> &input)) {
	/* if this template is instanciated the static_assert will fail but will give us an error message */
	static_assert(sizeof(hashAlgo) != sizeof(hashAlgo), "You must specialize HMAC function template");
	return std::vector<uint8_t>(0);
}

/* HMAC specialized template for SHA1 */
template <>
std::vector<uint8_t> HMAC<SHA1>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input) {
	std::vector<uint8_t> hmacOutput(SHA1::ssize());
	bctbx_hmacSha1(key.data(), key.size(), input.data(), input.size(), static_cast<uint8_t>(hmacOutput.size()),
	               hmacOutput.data());
	return hmacOutput;
}

/* HMAC specialized template for SHA256 */
template <>
std::vector<uint8_t> HMAC<SHA256>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input) {
	std::vector<uint8_t> hmacOutput(SHA256::ssize());
	bctbx_hmacSha256(key.data(), key.size(), input.data(), input.size(), static_cast<uint8_t>(hmacOutput.size()),
	                 hmacOutput.data());
	return hmacOutput;
}

/* HMAC specialized template for SHA384 */
template <>
std::vector<uint8_t> HMAC<SHA384>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input) {
	std::vector<uint8_t> hmacOutput(SHA384::ssize());
	bctbx_hmacSha384(key.data(), key.size(), input.data(), input.size(), static_cast<uint8_t>(hmacOutput.size()),
	                 hmacOutput.data());
	return hmacOutput;
}

/* HMAC specialized template for SHA512 */
template <>
std::vector<uint8_t> HMAC<SHA512>(const std::vector<uint8_t> &key, const std::vector<uint8_t> &input) {
	std::vector<uint8_t> hmacOutput(SHA512::ssize());
	bctbx_hmacSha512(key.data(), key.size(), input.data(), input.size(), static_cast<uint8_t>(hmacOutput.size()),
	                 hmacOutput.data());
	return hmacOutput;
}

/* HKDF templates */
/* HKDF must use a specialized template */
template <typename hashAlgo>
std::vector<uint8_t> HKDF(BCTBX_UNUSED(const std::vector<uint8_t> &salt),
                          BCTBX_UNUSED(const std::vector<uint8_t> &ikm),
                          BCTBX_UNUSED(const std::vector<uint8_t> &info),
                          BCTBX_UNUSED(size_t okmSize)) {
	/* if this template is instanciated the static_assert will fail but will give us an error message */
	static_assert(sizeof(hashAlgo) != sizeof(hashAlgo), "You must specialize HKDF function template");
	return std::vector<uint8_t>(0);
}

template <typename hashAlgo>
std::vector<uint8_t> HKDF(BCTBX_UNUSED(const std::vector<uint8_t> &salt),
                          BCTBX_UNUSED(const std::vector<uint8_t> &ikm),
                          BCTBX_UNUSED(const std::string &info),
                          BCTBX_UNUSED(size_t okmSize)) {
	/* if this template is instanciated the static_assert will fail but will give us an error message */
	static_assert(sizeof(hashAlgo) != sizeof(hashAlgo), "You must specialize HKDF function template");
	return std::vector<uint8_t>(0);
}

/*****************************************************************************/
/***                      Authenticated Encryption                         ***/
/*****************************************************************************/
/* AEAD template must be specialized */
template <typename AEADAlgo>
std::vector<uint8_t> AEADEncrypt(BCTBX_UNUSED(const std::vector<uint8_t> &key),
                                 BCTBX_UNUSED(const std::vector<uint8_t> IV),
                                 BCTBX_UNUSED(const std::vector<uint8_t> &plain),
                                 BCTBX_UNUSED(const std::vector<uint8_t> &AD),
                                 BCTBX_UNUSED(std::vector<uint8_t> &tag)) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty type
	 */
	static_assert(sizeof(AEADAlgo) != sizeof(AEADAlgo), "You must specialize AEADEncrypt function template");
	return std::vector<uint8_t>(0);
}

template <typename AEADAlgo>
bool AEADDecrypt(BCTBX_UNUSED(const std::vector<uint8_t> &key),
                 BCTBX_UNUSED(const std::vector<uint8_t> &IV),
                 BCTBX_UNUSED(const std::vector<uint8_t> &cipher),
                 BCTBX_UNUSED(const std::vector<uint8_t> &AD),
                 BCTBX_UNUSED(const std::vector<uint8_t> &tag),
                 BCTBX_UNUSED(std::vector<uint8_t> &plain)) {
	/* if this template is instanciated the static_assert will fail but will give us an error message with faulty type
	 */
	static_assert(sizeof(AEADAlgo) != sizeof(AEADAlgo), "You must specialize AEADEncrypt function template");
	return false;
}

/* declare AEAD template specialisations : AES256-GCM with 128 bits auth tag*/
template <>
std::vector<uint8_t> AEADEncrypt<AES256GCM128>(const std::vector<uint8_t> &key,
                                               const std::vector<uint8_t> &IV,
                                               const std::vector<uint8_t> &plain,
                                               const std::vector<uint8_t> &AD,
                                               std::vector<uint8_t> &tag) {
	// check key size (could have use array but Windows won't compile templates with constexpr functions result as
	// parameter)
	if (key.size() != AES256GCM128::keySize()) {
		throw BCTBX_EXCEPTION << "AEADEncrypt: Bad input parameter, key is expected to be " << AES256GCM128::keySize()
		                      << " bytes but " << key.size() << " provided";
	}
	tag.resize(AES256GCM128::tagSize());

	std::vector<uint8_t> cipher(plain.size());
	int ret = bctbx_aes_gcm_encrypt_and_tag(key.data(), key.size(), plain.data(), plain.size(), AD.data(), AD.size(),
	                                        IV.data(), IV.size(), tag.data(), tag.size(), cipher.data());

	if (ret != 0) {
		throw BCTBX_EXCEPTION << "Error during AES_GCM encryption : return value " << ret;
	}
	return cipher;
}

template <>
bool AEADDecrypt<AES256GCM128>(const std::vector<uint8_t> &key,
                               const std::vector<uint8_t> &IV,
                               const std::vector<uint8_t> &cipher,
                               const std::vector<uint8_t> &AD,
                               const std::vector<uint8_t> &tag,
                               std::vector<uint8_t> &plain) {
	// check key and tag size (could have use array but Windows won't compile templates with constexpr functions result
	// as parameter)
	if (key.size() != AES256GCM128::keySize()) {
		throw BCTBX_EXCEPTION << "AEADDecrypt: Bad input parameter, key is expected to be " << AES256GCM128::keySize()
		                      << " bytes but " << key.size() << " provided";
	}
	if (tag.size() != AES256GCM128::tagSize()) {
		throw BCTBX_EXCEPTION << "AEADDecrypt: Bad input parameter, tag is expected to be " << AES256GCM128::tagSize()
		                      << " bytes but " << tag.size() << " provided";
	}

	plain.resize(cipher.size()); // plain is the same size than cipher
	int ret = bctbx_aes_gcm_decrypt_and_auth(key.data(), key.size(), cipher.data(), cipher.size(), AD.data(), AD.size(),
	                                         IV.data(), IV.size(), tag.data(), tag.size(), plain.data());

	if (ret == 0) {
		return true;
	} else if (ret == BCTBX_ERROR_AUTHENTICATION_FAILED) {
		throw BCTBX_EXCEPTION << "Error during AES_GCM decryption : authentication failed";
	}
	throw BCTBX_EXCEPTION << "Error during AES_GCM decryption : return value " << ret;
}

} // namespace bctoolbox

/*****************************************************************************/
/***                      Random Number Generation                         ***/
/***                      C API                                            ***/
/*****************************************************************************/
/*** Random Number Generation: C API ***/
struct bctbx_rng_context_struct {
	bctoolbox::RNG m_rng;
};

extern "C" bctbx_rng_context_t *bctbx_rng_context_new(void) {
	bctbx_rng_context_t *context = new bctbx_rng_context_struct();
	return context;
}

extern "C" int32_t bctbx_rng_get(bctbx_rng_context_t *context, unsigned char *output, size_t output_length) {
	context->m_rng.randomize(output, output_length);
	return 0; // always return 0, in case of problem an exception is raised by randomize
}

extern "C" void bctbx_rng_context_free(bctbx_rng_context_t *context) {
	delete context;
}

/* bctbx_random returns a 32 bits random non suitable for cryptographic operation
 * its header is declared in port.h */
extern "C" unsigned int bctbx_random(void) {
	return bctoolbox::RNG::cRandomize();
}

extern "C" void bctbx_random_bytes(unsigned char *ret, size_t size) {
	bctoolbox::RNG::cRandomize(ret, size);
}
