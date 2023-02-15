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
#include "bctoolbox/crypto.h" // bctbx_clean
#include "bctoolbox/crypto.hh"
#include <algorithm>
#include <functional>

#include "bctoolbox/logging.h"
using namespace bctoolbox;
/**
 * Constants associated to this encryption module
 */

/** Chunk Header in this module holds: Auth tag(16 bytes), IV : 12 bytes, Encryption Counter 4 bytes
 */
static constexpr size_t chunkAuthTagSize = AES256GCM128::tagSize();
static constexpr size_t chunkIVSize = 12;
static constexpr size_t chunkHeaderSize = chunkAuthTagSize + chunkIVSize;
/**
 * File header holds: fileSalt (16 bytes), file header auth tag(32 bytes)
 */
static constexpr size_t fileSaltSize = 16;
static constexpr size_t fileAuthTagSize = 32;
static constexpr size_t fileHeaderSize = fileSaltSize + fileAuthTagSize;

/**
 * The master Key is expected to be 32 bytes
 */
static constexpr size_t masterKeySize = 32;

/** constructor called at file creation */
VfsEM_AES256GCM_SHA256::VfsEM_AES256GCM_SHA256()
    : mRNG(std::make_shared<bctoolbox::RNG>()), // start the local RNG
      mFileSalt(mRNG->randomize(fileSaltSize))  // generate a random file Salt
{
}

/** constructor called when opening an existing file */
VfsEM_AES256GCM_SHA256::VfsEM_AES256GCM_SHA256(const std::vector<uint8_t> &fileHeader)
    : mRNG(std::make_shared<bctoolbox::RNG>()), // start the local RNG
      mFileSalt(std::vector<uint8_t>(fileSaltSize)) {
	if (fileHeader.size() != fileHeaderSize) {
		throw EVFS_EXCEPTION << "The AES256GCM128-SHA256 encryption module expect a fileHeader of size "
		                     << fileHeaderSize << " bytes but " << fileHeader.size() << " are provided";
	}
	// File header Data is 32 bytes of integrity data, 16 bytes of global salt
	std::copy(fileHeader.cbegin(), fileHeader.cbegin() + fileAuthTagSize, mFileHeaderIntegrity.begin());
	std::copy(fileHeader.cbegin() + fileAuthTagSize, fileHeader.cend(), mFileSalt.begin());
}

/** destructor ensure proper cleaning of any key material **/
VfsEM_AES256GCM_SHA256::~VfsEM_AES256GCM_SHA256() {
	bctbx_clean(sMasterKey.data(), sMasterKey.size());
	bctbx_clean(sFileHeaderHMACKey.data(), sFileHeaderHMACKey.size());
}

const std::vector<uint8_t> VfsEM_AES256GCM_SHA256::getModuleFileHeader(const VfsEncryption &fileContext) const {
	if (sFileHeaderHMACKey.empty()) {
		throw EVFS_EXCEPTION
		    << "The AES256GCM128-SHA256 encryption module cannot generate its file header without master key";
	}
	// Only the actual file header is to authenticate, the module file header holds the global salt used to derive the
	// key feed to HMAC authenticating the file header so it is useless to authenticate it
	auto tag = HMAC<SHA256>(sFileHeaderHMACKey, fileContext.rawHeaderGet());

	// Append the actual file salt value to the tag
	auto ret = mFileSalt;
	ret.insert(ret.begin(), tag.cbegin(), tag.cend());
	return ret;
}

void VfsEM_AES256GCM_SHA256::setModuleSecretMaterial(const std::vector<uint8_t> &secret) {
	if (secret.size() != masterKeySize) {
		throw EVFS_EXCEPTION << "The AES256GCM128 SHA256 encryption module expect a secret material of size "
		                     << masterKeySize << " bytes but " << secret.size() << " are provided";
	}
	sMasterKey = secret;

	// Now that we have a master key, we can derive the header authentication one
	sFileHeaderHMACKey = bctoolbox::HKDF<SHA256>(mFileSalt, sMasterKey, "EVFS file Header", masterKeySize);
}

/**
 * Derive the key from master key for the given chunkIndex:
 * HKDF(fileSalt || ChunkIndex, master Key, "EVFS chunk")
 *
 * @param[in]	chunkIndex	the chunk index used in key derivation
 *
 * @return	the AES256-GCM128 key
 */
std::vector<uint8_t> VfsEM_AES256GCM_SHA256::deriveChunkKey(uint32_t chunkIndex) {
	std::vector<uint8_t> chunkSalt{mFileSalt};
	chunkSalt.push_back((chunkIndex >> 24) & 0xFF);
	chunkSalt.push_back((chunkIndex >> 16) & 0xFF);
	chunkSalt.push_back((chunkIndex >> 8) & 0xFF);
	chunkSalt.push_back(chunkIndex & 0xFF);
	return bctoolbox::HKDF<SHA256>(chunkSalt, sMasterKey, "EVFS chunk", AES256GCM128::keySize());
}

std::vector<uint8_t> VfsEM_AES256GCM_SHA256::decryptChunk(const uint32_t chunkIndex,
                                                          const std::vector<uint8_t> &rawChunk) {
	if (sMasterKey.empty()) {
		throw EVFS_EXCEPTION << "No encryption Master key set, cannot decrypt";
	}

	// derive the key : HKDF (fileHeaderSalt || Chunk Index, Master key, "EVFS chunk")
	std::vector<uint8_t> key{deriveChunkKey(chunkIndex)};

	// parse the header: tag, IV, encryption Counter
	std::vector<uint8_t> tag(AES256GCM128::tagSize());
	std::copy(rawChunk.cbegin(), rawChunk.cbegin() + chunkAuthTagSize, tag.begin());
	std::vector<uint8_t> IV(rawChunk.cbegin() + chunkAuthTagSize, rawChunk.cbegin() + chunkAuthTagSize + chunkIVSize);
	std::vector<uint8_t> AD{}; // No associated data

	// extract cipher
	std::vector<uint8_t> cipher(rawChunk.cbegin() + chunkHeaderSize, rawChunk.cend());

	// decrypt and auth
	std::vector<uint8_t> plain;
	if (AEADDecrypt<AES256GCM128>(key, IV, cipher, AD, tag, plain) == false) {
		throw EVFS_EXCEPTION << "Authentication failure during chunk decryption";
	}

	// cleaning
	bctbx_clean(key.data(), key.size());

	return plain;
}

// This module does not reuse any part of its chunk header during encryption
// So re-encryption is the same than initial encryption
void VfsEM_AES256GCM_SHA256::encryptChunk(const uint32_t chunkIndex,
                                          std::vector<uint8_t> &rawChunk,
                                          const std::vector<uint8_t> &plainData) {

	rawChunk = encryptChunk(chunkIndex, plainData);
}

std::vector<uint8_t> VfsEM_AES256GCM_SHA256::encryptChunk(const uint32_t chunkIndex,
                                                          const std::vector<uint8_t> &plainData) {
	if (sMasterKey.empty()) {
		throw EVFS_EXCEPTION << "No encryption Master key set, cannot encrypt";
	}
	// generate a random IV
	auto IV = mRNG->randomize(chunkIVSize);

	// derive the key : HKDF (fileHeaderSalt || Chunk Index, Master key, "EVFS chunk")
	std::vector<uint8_t> key{deriveChunkKey(chunkIndex)};

	std::vector<uint8_t> AD{};
	std::vector<uint8_t> tag(AES256GCM128::tagSize());
	std::vector<uint8_t> rawChunk = AEADEncrypt<AES256GCM128>(key, IV, plainData, AD, tag);

	// insert header:
	std::vector<uint8_t> chunkHeader(chunkHeaderSize, 0);
	std::copy(tag.cbegin(), tag.cend(), chunkHeader.begin());
	std::copy(IV.cbegin(), IV.cend(), chunkHeader.begin() + tag.size());
	rawChunk.insert(rawChunk.begin(), chunkHeader.cbegin(), chunkHeader.cend());

	// cleaning
	bctbx_clean(key.data(), key.size());

	return rawChunk;
}

/**
 * When this function is called, m_fileHeader holds the integrity tag read from file
 * and sFileHeaderHMACKey holds the derived key for header authentication
 * Compute the HMAC on the whole rawfileHeader + the module header
 * Check it match what we have in the m_fileHeader
 */
bool VfsEM_AES256GCM_SHA256::checkIntegrity(const VfsEncryption &fileContext) {
	if (sFileHeaderHMACKey.empty()) {
		throw EVFS_EXCEPTION
		    << "The AES256GCM128-SHA256 encryption module cannot generate its file header without master key";
	}
	// Only the actual file header is to authenticate, the module file header holds the global salt used to derive the
	// key feed to HMAC authenticating the file header so it is useless to authenticate it
	auto tag = HMAC<SHA256>(sFileHeaderHMACKey, fileContext.rawHeaderGet());

	return (std::equal(tag.cbegin(), tag.cend(), mFileHeaderIntegrity.cbegin()));
}
/**
 * This function exists as static and non static
 */
size_t VfsEM_AES256GCM_SHA256::moduleFileHeaderSize() noexcept {
	return fileHeaderSize;
}

/**
 * @return the size in bytes of the chunk header
 */
size_t VfsEM_AES256GCM_SHA256::getChunkHeaderSize() const noexcept {
	return chunkHeaderSize;
}
/**
 * @return the size in bytes of file header module data
 */
size_t VfsEM_AES256GCM_SHA256::getModuleFileHeaderSize() const noexcept {
	return fileHeaderSize;
}

/**
 * @return the secret material size
 */
size_t VfsEM_AES256GCM_SHA256::getSecretMaterialSize() const noexcept {
	return masterKeySize;
}
