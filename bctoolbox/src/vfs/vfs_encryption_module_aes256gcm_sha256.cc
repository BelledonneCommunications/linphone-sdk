/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
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

#include "vfs_encryption_module_aes256gcm_sha256.hh"
#include <algorithm>
#include <functional>
#include "bctoolbox/crypto.hh"
#include "bctoolbox/crypto.h" // bctbx_clean

#include "bctoolbox/logging.h"
using namespace bctoolbox;

/** constructor called at file creation */
VfsEM_AES256GCM_SHA256::VfsEM_AES256GCM_SHA256() :
	m_RNG(std::make_shared<bctoolbox::RNG>()), // start the local RNG
	m_fileSalt(m_RNG->randomize(fileSaltSize)) // generate a random file Salt
{
}

/** constructor called when opening an existing file */
VfsEM_AES256GCM_SHA256::VfsEM_AES256GCM_SHA256(const std::vector<uint8_t> &fileHeader) :
	m_RNG(std::make_shared<bctoolbox::RNG>()), // start the local RNG
	m_fileSalt(std::vector<uint8_t>(fileSaltSize))
{
	if (fileHeader.size() != fileHeaderSize) {
		throw EVFS_EXCEPTION<<"The AES256GCM128-SHA256 encryption module expect a fileHeader of size "<<fileHeaderSize<<" bytes but "<<fileHeader.size()<<" are provided";
	}
	// File header Data is 32 bytes of integrity data, 16 bytes of global salt
	std::copy(fileHeader.cbegin(), fileHeader.cbegin()+fileAuthTagSize, m_fileHeaderIntegrity.begin());
	std::copy(fileHeader.cbegin()+fileAuthTagSize, fileHeader.cend(), m_fileSalt.begin());
}

/** destructor ensure proper cleaning of any key material **/
VfsEM_AES256GCM_SHA256::~VfsEM_AES256GCM_SHA256() {
	bctbx_clean(s_masterKey.data(), s_masterKey.size());
	bctbx_clean(s_fileHeaderHMACKey.data(), s_fileHeaderHMACKey.size());
}

const std::vector<uint8_t> VfsEM_AES256GCM_SHA256::getModuleFileHeader(const VfsEncryption &fileContext) const {
	if (s_fileHeaderHMACKey.empty()) {
		throw EVFS_EXCEPTION<<"The AES256GCM128-SHA256 encryption module cannot generate its file header without master key";
	}
	// Only the actual file header is to authenticate, the module file header holds the global salt used to derive the key feed to HMAC authenticating the file header
	// so it is useless to authenticate it
	auto tag = HMAC<SHA256>(s_fileHeaderHMACKey, fileContext.r_getHeader());

	// Append the actual file salt value to the tag
	auto ret = m_fileSalt;
	ret.insert(ret.begin(), tag.cbegin(), tag.cend());
	return ret;
}

void VfsEM_AES256GCM_SHA256::setModuleSecretMaterial(const std::vector<uint8_t> &secret) {
	if (secret.size() != masterKeySize) {
		throw EVFS_EXCEPTION<<"The AES256GCM128 SHA256 encryption module expect a secret material of size "<<masterKeySize<<" bytes but "<<secret.size()<<" are provided";
	}
	s_masterKey = secret;

	// Now that we have a master key, we can derive the header authentication one
	s_fileHeaderHMACKey = bctoolbox::HKDF<SHA256>(m_fileSalt, s_masterKey, "EVFS file Header", masterKeySize);
}

/**
 * Derive the key from master key for the given chunkIndex:
 * HKDF(fileSalt || ChunkIndex, master Key, "EVFS chunk")
 *
 * @param[in]	chunkIndex	the chunk index used in key derivation
 *
 * @return	the AES256-GCM128 key
 */
const std::array<uint8_t, AES256GCM128::keySize()> VfsEM_AES256GCM_SHA256::deriveChunkKey(uint32_t chunkIndex) {
	auto chunkSalt{m_fileSalt};
	chunkSalt.push_back((chunkIndex>>24)&0xFF);
	chunkSalt.push_back((chunkIndex>>16)&0xFF);
	chunkSalt.push_back((chunkIndex>>8)&0xFF);
	chunkSalt.push_back(chunkIndex&0xFF);
	auto key = bctoolbox::HKDF<SHA256>(chunkSalt, s_masterKey, "EVFS chunk", AES256GCM128::keySize());
	std::array<uint8_t, AES256GCM128::keySize()> keyArray;
	std::move(key.cbegin(), key.cend(), keyArray.begin());
	return keyArray;
}

std::vector<uint8_t> VfsEM_AES256GCM_SHA256::decryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &rawChunk) {
	if (s_masterKey.empty()) {
		throw EVFS_EXCEPTION<<"No encryption Master key set, cannot decrypt";
	}

	// derive the key : HKDF (fileHeaderSalt || Chunk Index, Master key, "EVFS chunk")
	auto key {deriveChunkKey(chunkIndex)};

	// parse the header: tag, IV, encryption Counter
	std::array<uint8_t, AES256GCM128::tagSize()> tag;
	std::copy(rawChunk.cbegin(), rawChunk.cbegin()+chunkAuthTagSize, tag.begin());
	std::vector<uint8_t> IV(rawChunk.cbegin() + chunkAuthTagSize, rawChunk.cbegin() + chunkAuthTagSize + chunkIVSize);
	std::vector<uint8_t> AD{}; // No associated data

	// extract cipher
	std::vector<uint8_t> cipher(rawChunk.cbegin()+chunkHeaderSize, rawChunk.cend());

	// decrypt and auth
	std::vector<uint8_t> plain;
	if (AEAD_decrypt<AES256GCM128>(key, IV, cipher, AD, tag, plain) == false) {
		throw EVFS_EXCEPTION<<"Authentication failure during chunk decryption";
	}

	// cleaning
	bctbx_clean(key.data(), key.size());

	return plain;
}

// This module does not reuse any part of its chunk header during encryption
// So re-encryption is the same than initial encryption
void VfsEM_AES256GCM_SHA256::encryptChunk(const uint32_t chunkIndex, std::vector<uint8_t> &rawChunk, const std::vector<uint8_t> &plainData) {

	rawChunk = encryptChunk(chunkIndex, plainData);
}

std::vector<uint8_t> VfsEM_AES256GCM_SHA256::encryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &plainData) {
	if (s_masterKey.empty()) {
		throw EVFS_EXCEPTION<<"No encryption Master key set, cannot encrypt";
	}
	// generate a random IV
	auto IV = m_RNG->randomize(chunkIVSize);

	// derive the key : HKDF (fileHeaderSalt || Chunk Index, Master key, "EVFS chunk")
	auto key {deriveChunkKey(chunkIndex)};

	std::vector<uint8_t> AD{};
	std::array<uint8_t, AES256GCM128::tagSize()> tag;
	auto rawChunk = AEAD_encrypt<AES256GCM128>(key, IV, plainData, AD, tag);

	// insert header:
	std::vector<uint8_t> chunkHeader(chunkHeaderSize,0);
	std::copy(tag.cbegin(), tag.cend(), chunkHeader.begin());
	std::copy(IV.cbegin(), IV.cend(), chunkHeader.begin()+tag.size());
	rawChunk.insert(rawChunk.begin(), chunkHeader.cbegin(), chunkHeader.cend());

	// cleaning
	bctbx_clean(key.data(), key.size());

	return rawChunk;
}

/**
 * When this function is called, m_fileHeader holds the integrity tag read from file
 * and s_fileHeaderHMACKey holds the derived key for header authentication
 * Compute the HMAC on the whole rawfileHeader + the module header
 * Check it match what we have in the m_fileHeader
 */
bool VfsEM_AES256GCM_SHA256::checkIntegrity(const VfsEncryption &fileContext) {
	if (s_fileHeaderHMACKey.empty()) {
		throw EVFS_EXCEPTION<<"The AES256GCM128-SHA256 encryption module cannot generate its file header without master key";
	}
	// Only the actual file header is to authenticate, the module file header holds the global salt used to derive the key feed to HMAC authenticating the file header
	// so it is useless to authenticate it
	auto tag = HMAC<SHA256>(s_fileHeaderHMACKey, fileContext.r_getHeader());

	return (std::equal(tag.cbegin(), tag.cend(), m_fileHeaderIntegrity.cbegin()));
}
