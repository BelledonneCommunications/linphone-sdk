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
#include <algorithm>
#include <functional>
#include "bctoolbox/logging.h"
#include "bctoolbox/crypto.h"
using namespace bctoolbox;

static std::string getHex(const std::vector<uint8_t>& v)
{
    std::string result;
    result.reserve(v.size() * 2);   // two digits per character

    static constexpr char hex[] = "0123456789ABCDEF";

    for (uint8_t c : v)
    {
        result.push_back(hex[c / 16]);
        result.push_back(hex[c % 16]);
    }

    return result;
}

VfsEncryptionModuleDummy::VfsEncryptionModuleDummy() {
	m_fileHeader = std::vector<uint8_t>{0xaa, 0x55, 0xbb, 0x44, 0xcc, 0x33, 0xdd, 0x22}; // this is a constant for the dummy suite to help debug, real module would do otherwise
	m_secret = std::vector<uint8_t>{};
}

void VfsEncryptionModuleDummy::setModuleFileHeader(std::vector<uint8_t> &fileHeader) {
	if (fileHeader.size() != fileHeaderSize) {
		throw EVFS_EXCEPTION<<"The dummy encryption module expect a fileHeader of size "<<fileHeaderSize<<" bytes but "<<fileHeader.size()<<" are provided";
	}
	m_fileHeader = fileHeader;
}

std::vector<uint8_t> VfsEncryptionModuleDummy::getModuleFileHeader() const noexcept {
	return m_fileHeader;
}

void VfsEncryptionModuleDummy::setModuleSecretMaterial(const std::vector<uint8_t> &secret) {
	if (secret.size() != secretMaterialSize) {
		throw EVFS_EXCEPTION<<"The dummy encryption module expect a secret material of size "<<secretMaterialSize<<" bytes but "<<secret.size()<<" are provided";
	}
	m_secret = secret;
}

std::vector<uint8_t> VfsEncryptionModuleDummy::decryptChunk(const std::vector<uint8_t> &rawChunk) {
	// First check the integrity of the block. In the dummy module, integrity is 8 bytes of HMAC SHA256 keyed with the master key
	std::vector<uint8_t> computedIntegrity = chunkIntegrityTag(rawChunk);
	if (!std::equal(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.cbegin())) {
		throw EVFS_EXCEPTION<<"Integrity check failure while decrypting";
	}

	std::vector<uint8_t> plainData(rawChunk.cbegin()+chunkHeaderSize, rawChunk.cend());
	// The dummy decryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8 bytes)
	// The 16 bytes result is then xor with the secret material
	std::vector<uint8_t> XORkey(getModuleFileHeader()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin()+8, rawChunk.cbegin()+chunkHeaderSize); // and chunkHeaderMaterial
	std::transform(XORkey.begin(), XORkey.end(), m_secret.cbegin(), XORkey.begin(), std::bit_xor<uint8_t>());

	BCTBX_SLOGD<<"JOHAN: decryptChunk :"<<std::endl<<"   chunk is "<<getHex(plainData)<<std::endl<<"   key is "<<getHex(XORkey);
	// Xor it all, 16 bytes at a time
	for (size_t i=0; i<plainData.size(); i+=16) {
		std::transform(plainData.begin()+i, plainData.begin()+std::min(i+16,plainData.size()), XORkey.cbegin(), plainData.begin()+i, std::bit_xor<uint8_t>());
	}
	BCTBX_SLOGD<<"JOHAN: decryptChunk :"<<std::endl<<"   output is "<<getHex(plainData);

	return plainData;
}

void VfsEncryptionModuleDummy::encryptChunk(std::vector<uint8_t> &rawChunk, const std::vector<uint8_t> &plainData) {
	BCTBX_SLOGD<<"JOHAN: encryptChunk re :"<<std::endl<<"   plain is "<<plainData.size()<<std::endl<<"    plain: "<<getHex(plainData);
	BCTBX_SLOGD<<"    in cipher: "<<getHex(rawChunk);

	// Check integrity on the whole block. Actual module shall optimize it and be able to check only the header
	std::vector<uint8_t> computedIntegrity = chunkIntegrityTag(rawChunk);
	if (!std::equal(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.cbegin())) {
		throw EVFS_EXCEPTION<<"Integrity check failure while re-encrypting chunk";
	}


	// Increase the encryption count
	uint32_t encryptionCount = rawChunk[12]<<24 | rawChunk[13]<<16 | rawChunk[14]<<8 | rawChunk[15];
	encryptionCount++;
	rawChunk[12] = (encryptionCount>>24)&0xFF;
	rawChunk[13] = (encryptionCount>>16)&0xFF;
	rawChunk[14] = (encryptionCount>>8)&0xFF;
	rawChunk[15] = (encryptionCount&0xFF);

	// resize encrypted buffer
	rawChunk.resize(chunkHeaderSize+plainData.size());

	// The dummy encryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8 bytes, the part after the integrity tag)
	// The 16 bytes result is then xor with the secret material
	std::vector<uint8_t> XORkey(getModuleFileHeader()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin()+8, rawChunk.cbegin()+chunkHeaderSize); // and chunkHeaderMaterial
	std::transform(XORkey.begin(), XORkey.end(), m_secret.cbegin(), XORkey.begin(), std::bit_xor<uint8_t>());

	// Xor it all, 16 bytes at a time
	for (size_t i=0; i<plainData.size(); i+=16) {
		std::transform(plainData.begin()+i, plainData.begin()+std::min(i+16,plainData.size()), XORkey.cbegin(), rawChunk.begin()+chunkHeaderSize+i, std::bit_xor<uint8_t>());
	}

	// Update integrity
	computedIntegrity = chunkIntegrityTag(rawChunk);
	std::copy(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.begin());

	BCTBX_SLOGD<<"   out cipher: "<<getHex(rawChunk);
}

std::vector<uint8_t> VfsEncryptionModuleDummy::encryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &plainData) {
	BCTBX_SLOGD<<"JOHAN: encryptChunk new :"<<std::endl<<"   plain is "<<plainData.size()<<" index is "<<chunkIndex<<std::endl<<"    plain: "<<getHex(plainData);
	// create a vector of the appropriate size, init to 0
	std::vector<uint8_t> rawChunk(chunkHeaderSize+plainData.size(), 0);

	// set in the chunk Index
	rawChunk[8] = (chunkIndex>>24)&0xFF;
	rawChunk[9] = (chunkIndex>>16)&0xFF;
	rawChunk[10] = (chunkIndex>>8)&0xFF;
	rawChunk[11] = (chunkIndex&0xFF);
	// rawChunk 12 to 15 is the encryptionCount, 0 is fine

	// The dummy encryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8 bytes, the part after the integrity tag)
	// The 16 bytes result is then xor with the secret material
	std::vector<uint8_t> XORkey(getModuleFileHeader()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin()+8, rawChunk.cbegin()+chunkHeaderSize); // and chunkHeaderMaterial
	std::transform(XORkey.begin(), XORkey.end(), m_secret.cbegin(), XORkey.begin(), std::bit_xor<uint8_t>());

	// Xor it all, 16 bytes at a time
	for (size_t i=0; i<plainData.size(); i+=16) {
		std::transform(plainData.begin()+i, plainData.begin()+std::min(i+16,plainData.size()), XORkey.cbegin(), rawChunk.begin()+chunkHeaderSize+i, std::bit_xor<uint8_t>());
	}

	// Update integrity
	auto computedIntegrity = chunkIntegrityTag(rawChunk);
	std::copy(computedIntegrity.cbegin(), computedIntegrity.cend(), rawChunk.begin());

	BCTBX_SLOGD<<"    cipher: "<<getHex(rawChunk);

	return rawChunk;
}

bool VfsEncryptionModuleDummy::checkIntegrity(const VfsEncryption &fileContext) {
	BCTBX_SLOGD<<"JOHAN: checking file integrity for "<<fileContext.filename_get();
	return true;
}

std::vector<uint8_t> VfsEncryptionModuleDummy::chunkIntegrityTag(const std::vector<uint8_t> &chunk) {
	std::vector<uint8_t> tag(8);
	bctbx_hmacSha256(m_secret.data(), secretMaterialSize,
		chunk.data()+8, // compute integrity on the whole block (header included) but skip the integrity tag (8 first bytes)
		chunk.size()-8,
		8, // get 8 bytes out of the HMAC
		tag.data());
	return tag;
}
