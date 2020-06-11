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

void VfsEncryptionModuleDummy::setModuleFileHeader(std::vector<uint8_t> &fileHeader) {
	if (fileHeader.size() != fileHeaderSize) {
		//TODO: throw an exception?
	}
	m_fileHeader = fileHeader;
}

std::vector<uint8_t> VfsEncryptionModuleDummy::getModuleFileHeader() {
	/* at file creation we won't have any fileHeader, create it */
	if (m_fileHeader.size()==0) {
		m_fileHeader = std::vector<uint8_t>{0xaa, 0x55, 0xbb, 0x44, 0xcc, 0x33, 0xdd, 0x22}; // this is a constant for the dummy suite to help debug, real module would do otherwise
	}
	return m_fileHeader;
}

std::vector<uint8_t> VfsEncryptionModuleDummy::decryptChunk(const std::vector<uint8_t> &rawChunk) {
	// The dummy decryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8 bytes)
	std::vector<uint8_t> plainData(rawChunk.cbegin()+chunkHeaderSize, rawChunk.cend());
	std::vector<uint8_t> XORkey(getModuleFileHeader()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin(), rawChunk.cbegin()+chunkHeaderSize); // and chunkHeaderMaterial

	BCTBX_SLOGD<<"JOHAN: decryptChunk :"<<std::endl<<"   chunk is "<<getHex(plainData)<<std::endl<<"   key is "<<getHex(XORkey);
	// Xor it all, 16 bytes at a time
	for (size_t i=0; i<plainData.size(); i+=16) {
		std::transform(plainData.begin()+i, plainData.begin()+std::min(i+16,plainData.size()), XORkey.cbegin(), plainData.begin()+i, std::bit_xor<uint8_t>());
	}
	BCTBX_SLOGD<<"JOHAN: decryptChunk :"<<std::endl<<"   output is "<<getHex(plainData);

	return plainData;

	// There is no authentication on this dummy module, but it shall take place here
}

void VfsEncryptionModuleDummy::encryptChunk(std::vector<uint8_t> &rawChunk, const std::vector<uint8_t> &plainData) {
	// Chunkheader shall be authenticated in some way and this authentication checked here
	BCTBX_SLOGD<<"JOHAN: encryptChunk re :"<<std::endl<<"   plain is "<<plainData.size()<<std::endl<<"    plain: "<<getHex(plainData);
	BCTBX_SLOGD<<"    in cipher: "<<getHex(rawChunk);

	// Increase the encryption count
	uint32_t encryptionCount = rawChunk[4]<<24 | rawChunk[5]<<16 | rawChunk[6]<<8 | rawChunk[7];
	encryptionCount++;
	rawChunk[4] = (encryptionCount>>24)&0xFF;
	rawChunk[5] = (encryptionCount>>16)&0xFF;
	rawChunk[6] = (encryptionCount>>8)&0xFF;
	rawChunk[7] = (encryptionCount&0xFF);

	// resize encrypted buffer
	rawChunk.resize(chunkHeaderSize+plainData.size());

	// The dummy encryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8 bytes)
	std::vector<uint8_t> XORkey(getModuleFileHeader()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin(), rawChunk.cbegin()+chunkHeaderSize); // and chunkHeaderMaterial

	// Xor it all, 16 bytes at a time
	for (size_t i=0; i<plainData.size(); i+=16) {
		std::transform(plainData.begin()+i, plainData.begin()+std::min(i+16,plainData.size()), XORkey.cbegin(), rawChunk.begin()+chunkHeaderSize+i, std::bit_xor<uint8_t>());
	}

	// There is no authentication on this dummy module, but it shall be part of the encryption and set in the chunk header
	BCTBX_SLOGD<<"    out cipher: "<<getHex(rawChunk);
}

std::vector<uint8_t> VfsEncryptionModuleDummy::encryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &plainData) {
	BCTBX_SLOGD<<"JOHAN: encryptChunk new :"<<std::endl<<"   plain is "<<plainData.size()<<" index is "<<chunkIndex<<std::endl<<"    plain: "<<getHex(plainData);
	// create a vector of the appropriate size, init to 0
	std::vector<uint8_t> rawChunk(chunkHeaderSize+plainData.size(), 0);

	// set in the chunk Index
	rawChunk[0] = (chunkIndex>>24)&0xFF;
	rawChunk[1] = (chunkIndex>>16)&0xFF;
	rawChunk[2] = (chunkIndex>>8)&0xFF;
	rawChunk[3] = (chunkIndex&0xFF);
	// rawChunk 4 to 7 is the encryptionCount, 0 is fine

	// The dummy encryption is a simple XOR on 16 bytes blocks with fileHeaderMaterial(8 bytes)||chunkHeaderMaterial(8 bytes)
	std::vector<uint8_t> XORkey(getModuleFileHeader()); // Xor key is file header material
	XORkey.insert(XORkey.end(), rawChunk.cbegin(), rawChunk.cbegin()+chunkHeaderSize); // and chunkHeaderMaterial

	// Xor it all, 16 bytes at a time
	for (size_t i=0; i<plainData.size(); i+=16) {
		std::transform(plainData.begin()+i, plainData.begin()+std::min(i+16,plainData.size()), XORkey.cbegin(), rawChunk.begin()+chunkHeaderSize+i, std::bit_xor<uint8_t>());
	}
	BCTBX_SLOGD<<"    cipher: "<<getHex(rawChunk);

	return rawChunk;
}
