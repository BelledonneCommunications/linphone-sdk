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

#include "vfs_encryption_module_dummy.hh"
#include "bctoolbox/crypto.h"
#include "bctoolbox/logging.h"
#include <algorithm>
#include <functional>
using namespace bctoolbox;

// someone does include the evil windef.h so we must undef the min and max macros to be able to use std::min and
// std::max
#undef min
#undef max

/**
 * Constant associated to this encryption module
 */

/** Header in the dummy module holds:
 * - Integrity tag 8 bytes. The HMACSHA256 of the block content and header (excluding this tag)
 * - Block Index : 4 bytes. (-> max 4 giga blocks in a file -> more than enough)
 * - Encryption Counter : 4 bytes counter (increased at each encryption)
 * Total size : 16 bytes
 */
static constexpr size_t chunkHeaderSize = 16;
/**
 * The dummy module file header holds:
 * - fixed Random IV : 8 bytes
 * - Integrity tag 8 bytes. The HMACSHA256 of the file header - (including the begining of the module file header first
 * 8 bytes, excluding this tag)
 */
static constexpr size_t fileHeaderSize = 16;

/**
 * The dummy module secret material is a key used to Xor blocks
 * - size is 16 bytes
 */
static constexpr size_t secretMaterialSize = 16;

static std::string getHex(const std::vector<uint8_t> &v) {
	std::string result;
	result.reserve(v.size() * 2); // two digits per character

	static constexpr char hex[] = "0123456789ABCDEF";

	for (uint8_t c : v) {
		result.push_back(hex[c / 16]);
		result.push_back(hex[c % 16]);
	}

	return result;
}

// chunk index is in chunk 8,9,10,11
uint32_t VfsEncryptionModuleDummy::getChunkIndex(const std::vector<uint8_t> &chunk) const {
	return chunk[8] << 24 | chunk[9] << 16 | chunk[10] << 8 | chunk[11];
}

/**
 * Get global IV. Part of IV common to all chunks
 */
std::vector<uint8_t> VfsEncryptionModuleDummy::globalIV() const {
	return mFileHeader;
}

VfsEncryptionModuleDummy::VfsEncryptionModuleDummy() {
	// this is a constant for the dummy suite to help debug, real module would do otherwise
	// the fileHeader also holds a integrity part computed on the whole fileHeader in the get function
	mFileHeader = std::vector<uint8_t>{0xaa, 0x55, 0xbb, 0x44, 0xcc, 0x33, 0xdd, 0x22};
	mFileHeaderIntegrity.resize(8);
	mSecret = std::vector<uint8_t>{};
}

VfsEncryptionModuleDummy::VfsEncryptionModuleDummy(const std::vector<uint8_t> &fileHeader) {
	mSecret = std::vector<uint8_t>{};
	mFileHeader.resize(8);
	mFileHeaderIntegrity.resize(8);

	if (fileHeader.size() != fileHeaderSize) {
		throw EVFS_EXCEPTION << "The dummy encryption module expect a fileHeader of size " << fileHeaderSize
		                     << " bytes but " << fileHeader.size() << " are provided";
	}
	// File header Data is 8 bytes of integrity data, 8 bytes of actual header (a global IV)
	std::copy(fileHeader.cbegin(), fileHeader.cbegin() + 8, mFileHeaderIntegrity.begin());
	std::copy(fileHeader.cbegin() + 8, fileHeader.cend(), mFileHeader.begin());
}

const std::vector<uint8_t> VfsEncryptionModuleDummy::getModuleFileHeader(const VfsEncryption &fileContext) const {
	// Update the integrity on fileHeader
	auto header = fileContext.rawHeaderGet();
	// append the part of the module file header we want to authentify
	auto moduleAuthentifiedPart = globalIV();
	header.insert(header.end(), moduleAuthentifiedPart.cbegin(), moduleAuthentifiedPart.cend());
	std::vector<uint8_t> tag(8);
	// Compute HMAC keyed with global key
	bctbx_hmacSha256(mSecret.data(), secretMaterialSize, header.data(), header.size(),
	                 8, // get 8 bytes out of the HMAC
	                 tag.data());

	// Append the actual file header value to the tag
	tag.insert(tag.end(), mFileHeader.cbegin(), mFileHeader.cend());
	BCTBX_SLOGD << "get Module file header returns " << getHex(tag) << std::endl
	            << " Key " << getHex(mSecret) << std::endl
	            << " Header " << getHex(header);
	return tag;
}

void VfsEncryptionModuleDummy::setModuleSecretMaterial(const std::vector<uint8_t> &secret) {
	if (secret.size() != secretMaterialSize) {
		throw EVFS_EXCEPTION << "The dummy encryption module expect a secret material of size " << secretMaterialSize
		                     << " bytes but " << secret.size() << " are provided";
	}
	mSecret = secret;
}

std::vector<uint8_t> VfsEncryptionModuleDummy::decryptChunk(const uint32_t chunkIndex,
                                                            const std::vector<uint8_t> &rawChunk) {
	// First check the integrity of the block. In the dummy module, integrity is 8 bytes of HMAC SHA256 keyed with the
	// master key
	std::vector<uint8_t> computedIntegrity = chunkIntegrityTag(rawChunk);
	if (!std::equal(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.cbegin())) {
		throw EVFS_EXCEPTION << "Integrity check failure while decrypting";
	}

	// Check the given chunk index is matching the one found in block - avoid attacker moving blocks in the file
	if (chunkIndex != getChunkIndex(rawChunk)) {
		throw EVFS_EXCEPTION << "Integrity check: unmatching chunk index";
	}

	std::vector<uint8_t> plainData(rawChunk.cbegin() + chunkHeaderSize, rawChunk.cend());
	// The dummy decryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8
	// bytes) The 16 bytes result is then xor with the secret material
	std::vector<uint8_t> XORkey(globalIV()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin() + 8, rawChunk.cbegin() + chunkHeaderSize); // and chunkHeaderMaterial
	std::transform(XORkey.begin(), XORkey.end(), mSecret.cbegin(), XORkey.begin(), std::bit_xor<uint8_t>());

	BCTBX_SLOGD << "decryptChunk :" << std::endl
	            << "   chunk is " << getHex(plainData) << std::endl
	            << "   key is " << getHex(XORkey);
	// Xor it all, 16 bytes at a time
	for (size_t i = 0; i < plainData.size(); i += 16) {
		std::transform(plainData.begin() + i, plainData.begin() + std::min(i + 16, plainData.size()), XORkey.cbegin(),
		               plainData.begin() + i, std::bit_xor<uint8_t>());
	}
	BCTBX_SLOGD << "decryptChunk :" << std::endl << "   output is " << getHex(plainData);

	return plainData;
}

void VfsEncryptionModuleDummy::encryptChunk(const uint32_t chunkIndex,
                                            std::vector<uint8_t> &rawChunk,
                                            const std::vector<uint8_t> &plainData) {
	BCTBX_SLOGD << "encryptChunk re :" << std::endl
	            << "   plain is " << plainData.size() << std::endl
	            << "    plain: " << getHex(plainData);
	BCTBX_SLOGD << "    in cipher: " << getHex(rawChunk);

	// Check integrity on the whole block. Actual module shall optimize it and be able to check only the header
	// integrity, we just want to make sure the data we intend to use - header meta data - are valid
	std::vector<uint8_t> computedIntegrity = chunkIntegrityTag(rawChunk);
	if (!std::equal(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.cbegin())) {
		throw EVFS_EXCEPTION << "Integrity check failure while re-encrypting chunk";
	}
	// Check the given chunk index is matching the one found in block - avoid attacker moving blocks in the file
	if (chunkIndex != getChunkIndex(rawChunk)) {
		throw EVFS_EXCEPTION << "Integrity check: unmatching chunk index";
	}

	// Increase the encryption count
	uint32_t encryptionCount = rawChunk[12] << 24 | rawChunk[13] << 16 | rawChunk[14] << 8 | rawChunk[15];
	encryptionCount++;
	rawChunk[12] = (encryptionCount >> 24) & 0xFF;
	rawChunk[13] = (encryptionCount >> 16) & 0xFF;
	rawChunk[14] = (encryptionCount >> 8) & 0xFF;
	rawChunk[15] = (encryptionCount & 0xFF);

	// resize encrypted buffer
	rawChunk.resize(chunkHeaderSize + plainData.size());

	// The dummy encryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8
	// bytes, the part after the integrity tag) The 16 bytes result is then xor with the secret material
	std::vector<uint8_t> XORkey(globalIV()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin() + 8, rawChunk.cbegin() + chunkHeaderSize); // and chunkHeaderMaterial
	std::transform(XORkey.begin(), XORkey.end(), mSecret.cbegin(), XORkey.begin(), std::bit_xor<uint8_t>());

	// Xor it all, 16 bytes at a time
	for (size_t i = 0; i < plainData.size(); i += 16) {
		std::transform(plainData.begin() + i, plainData.begin() + std::min(i + 16, plainData.size()), XORkey.cbegin(),
		               rawChunk.begin() + chunkHeaderSize + i, std::bit_xor<uint8_t>());
	}

	// Update integrity
	computedIntegrity = chunkIntegrityTag(rawChunk);
	std::copy(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.begin());

	BCTBX_SLOGD << "   out cipher: " << getHex(rawChunk);
}

std::vector<uint8_t> VfsEncryptionModuleDummy::encryptChunk(const uint32_t chunkIndex,
                                                            const std::vector<uint8_t> &plainData) {
	BCTBX_SLOGD << "encryptChunk new :" << std::endl
	            << "   plain is " << plainData.size() << " index is " << chunkIndex << std::endl
	            << "    plain: " << getHex(plainData);
	// create a vector of the appropriate size, init to 0
	std::vector<uint8_t> rawChunk(chunkHeaderSize + plainData.size(), 0);

	// set in the chunk Index
	rawChunk[8] = (chunkIndex >> 24) & 0xFF;
	rawChunk[9] = (chunkIndex >> 16) & 0xFF;
	rawChunk[10] = (chunkIndex >> 8) & 0xFF;
	rawChunk[11] = (chunkIndex & 0xFF);
	// rawChunk 12 to 15 is the encryptionCount, 0 is fine

	// The dummy encryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8
	// bytes, the part after the integrity tag) The 16 bytes result is then xor with the secret material
	std::vector<uint8_t> XORkey(globalIV()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin() + 8, rawChunk.cbegin() + chunkHeaderSize); // and chunkHeaderMaterial
	std::transform(XORkey.begin(), XORkey.end(), mSecret.cbegin(), XORkey.begin(), std::bit_xor<uint8_t>());

	// Xor it all, 16 bytes at a time
	for (size_t i = 0; i < plainData.size(); i += 16) {
		std::transform(plainData.begin() + i, plainData.begin() + std::min(i + 16, plainData.size()), XORkey.cbegin(),
		               rawChunk.begin() + chunkHeaderSize + i, std::bit_xor<uint8_t>());
	}

	// Update integrity
	auto computedIntegrity = chunkIntegrityTag(rawChunk);
	std::copy(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.begin());

	BCTBX_SLOGD << "    cipher: " << getHex(rawChunk);

	return rawChunk;
}

/**
 * When this function is called, mFileHeader holds the integrity tag in its 8 first bytes
 * Compute the HMAC on the whole rawfileHeader + the module header
 * Check it match what we have in the mFileHeader
 */
bool VfsEncryptionModuleDummy::checkIntegrity(const VfsEncryption &fileContext) {
	// Integrity is performed on the header only - each chunk take care of its own
	auto header = fileContext.rawHeaderGet();
	// append the part of the module file header we want to authentify
	auto moduleAuthentifiedPart = globalIV();
	header.insert(header.end(), moduleAuthentifiedPart.cbegin(), moduleAuthentifiedPart.cend());
	std::vector<uint8_t> tag(8);
	// Compute HMAC keyed with global key
	bctbx_hmacSha256(mSecret.data(), secretMaterialSize, header.data(), header.size(),
	                 8, // get 8 bytes out of the HMAC
	                 tag.data());
	BCTBX_SLOGD << "check integrity compute  " << getHex(tag) << std::endl
	            << " Key " << getHex(mSecret) << std::endl
	            << " Header " << getHex(header);

	return (std::equal(tag.cbegin(), tag.cend(), mFileHeaderIntegrity.cbegin()));
}

std::vector<uint8_t> VfsEncryptionModuleDummy::chunkIntegrityTag(const std::vector<uint8_t> &chunk) const {
	std::vector<uint8_t> tag(8);
	bctbx_hmacSha256(
	    mSecret.data(), secretMaterialSize,
	    chunk.data() +
	        8, // compute integrity on the whole block (header included) but skip the integrity tag (8 first bytes)
	    chunk.size() - 8,
	    8, // get 8 bytes out of the HMAC
	    tag.data());
	return tag;
}

/**
 * @return the size in bytes of file header module data
 */
size_t VfsEncryptionModuleDummy::moduleFileHeaderSize() noexcept {
	return fileHeaderSize;
}
/**
 * @return the size in bytes of the chunk header
 */
size_t VfsEncryptionModuleDummy::getChunkHeaderSize() const noexcept {
	return chunkHeaderSize;
}
/**
 * @return the size in bytes of file header module data
 */
size_t VfsEncryptionModuleDummy::getModuleFileHeaderSize() const noexcept {
	return fileHeaderSize;
}

/**
 * @return the secret material size
 */
size_t VfsEncryptionModuleDummy::getSecretMaterialSize() const noexcept {
	return secretMaterialSize;
}
