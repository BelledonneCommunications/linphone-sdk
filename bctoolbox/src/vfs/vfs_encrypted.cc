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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/vfs.h"
#include "bctoolbox/vfs_encrypted.hh"
#include "vfs_encryption_module.hh"
#include "vfs_encryption_module_dummy.hh"
#include "vfs_encryption_module_aes256gcm_sha256.hh"
#include "bctoolbox/vfs_standard.h"
#include "bctoolbox/logging.h"


using namespace bctoolbox;

/** Helpers function: this part of the code must be updated to add modules */
static size_t moduleFileHeaderSize(const uint16_t suite) {
	switch (suite) {
		case static_cast<uint16_t>(EncryptionSuite::dummy):
			return VfsEncryptionModuleDummy::moduleFileHeaderSize();
		case static_cast<uint16_t>(EncryptionSuite::aes256gcm128_sha256):
			return VfsEM_AES256GCM_SHA256::moduleFileHeaderSize();
		case static_cast<uint16_t>(EncryptionSuite::unset):
		case static_cast<uint16_t>(EncryptionSuite::plain):
		default:
			return 0;
	}
}

// is called at file creation when the encryption suite is set using encryptionSuite_set
static std::shared_ptr<VfsEncryptionModule> make_VfsEncryptionModule(const EncryptionSuite suite) {
	switch (suite) {
		case EncryptionSuite::dummy:
			return std::make_shared<VfsEncryptionModuleDummy>();
		case EncryptionSuite::aes256gcm128_sha256:
			return std::make_shared<VfsEM_AES256GCM_SHA256>();
		case EncryptionSuite::plain:
			return nullptr;
		case EncryptionSuite::unset:
		default:
			throw EVFS_EXCEPTION<<"Encrypted FS: unsupported encryption scheme "<<static_cast<uint16_t>(suite);
	}
}
// is called when an encrypted file header is parsed
static std::shared_ptr<VfsEncryptionModule> make_VfsEncryptionModule(const uint16_t suite, const std::vector<uint8_t> moduleFileHeader) {
	switch (suite) {
		/* all supported scheme must be listed here */
		case static_cast<uint16_t>(EncryptionSuite::dummy):
			return std::make_shared<VfsEncryptionModuleDummy>(moduleFileHeader);
		case static_cast<uint16_t>(EncryptionSuite::aes256gcm128_sha256):
			return std::make_shared<VfsEM_AES256GCM_SHA256>(moduleFileHeader);
		case static_cast<uint16_t>(EncryptionSuite::unset):
		case static_cast<uint16_t>(EncryptionSuite::plain):
		default:
			throw EVFS_EXCEPTION<<"Encrypted FS: unsupported encryption scheme "<<suite;
	}
}

/***************************************************/
/* All file encryption scheme get a header:
 * - This part is fixed at file creation (or re-encoding)
 *    - magicnumber: hexadecimal ASCII for bcEncryptedFs: 0x6263456E637279707465644673 : 13 bytes
 *    - version Number: 0xMMmm : 2 bytes
 *    - encryption suite: 2 bytes
 *    - chunk size : 2 bytes : number of 16 bytes blocks in a file chunk - excluding chunk header if any.
 *    - Header extension size: 2 bytes. Flexibility on header size - so older version of parser may be able to read newer versions
 *    - size: clear text file size : 8 bytes.
 *    - [Optionnal Header Extension]
 *    - [Optionnal Encryption module data - size is given by the encryption suite selected]
 *
 * base header size is 29 bytes
 */
static const std::vector<uint8_t> BCENCRYPTEDFS={0x62, 0x63, 0x45, 0x6e, 0x63, 0x72, 0x79, 0x70, 0x74, 0x65, 0x64, 0x46, 0x73};
static constexpr uint16_t BcEncFS_v0100=0x0100;
/* header cannot be less than this size, even for an empty file */
static constexpr int64_t baseFileHeaderSize=29;

static constexpr size_t defaultChunkSize = 4096; // default chunk size in bytes


/**
 * Initialiase the static callback property
 */
EncryptedVfsOpenCb VfsEncryption::s_openCallback = nullptr;

VfsEncryption::VfsEncryption(bctbx_vfs_file_t *stdFp, const std::string &filename) : m_filename(filename), pFileStd(stdFp) {

	if (stdFp == NULL) throw EVFS_EXCEPTION<<"Cannot create a vfs encrytion object, vfs pointer is null";
	/* initialise it */
	m_versionNumber = BcEncFS_v0100; // default version number is the last available
	m_chunkSize = 0; // set to 0 at creation, is will be populated by parseHeader if there is one. If we are creating a file, let a chance to the callback to set the chunk size.
	m_module = nullptr;
	m_headerExtensionSize = 0;
	m_fileSize = 0;


	// If the file exists, read the header to check it is an encrypted file and gets its encrypted policy
	// if the file is plain, set the m_fileSize
	bool createFile = true;
	if (bctbx_file_size(stdFp) > 0) {
		parseHeader();
		createFile = false;
	}

	/* if the static callback is set, call it */
	if (VfsEncryption::openCallback_get() != nullptr) {
		(VfsEncryption::openCallback_get())(*this);
	} else {
		throw EVFS_EXCEPTION << "Encrypted VFS: must provide a callback to setup key material";
	}

	if (m_module == nullptr) { // this is a plain file, nothing else to do
		return;
	}

	/* check we have a valid chunk size */
	if (m_chunkSize == 0) { // this is a file creation and the callback didn't set it
		m_chunkSize = defaultChunkSize; // assign the default one
	}

	/*  now we shall have all the material (settings and keys ) to check the file integrity */
	if (m_fileSize > 0 ) { // this is not a file creation
		if (m_module->checkIntegrity(*this) != true) {
			throw EVFS_EXCEPTION<<"Integrity check fail while opening file "<<m_filename;
		}
	}

	if (createFile) {
		writeHeader();
	}
}

VfsEncryption::~VfsEncryption() {
	if (pFileStd != nullptr) {
		bctbx_file_close(pFileStd);
	}
}



/**
 * Returns the size of chunks in which the file is divided for encryption
 */
size_t VfsEncryption::chunkSize_get() const noexcept {
	return (m_chunkSize==0?defaultChunkSize:m_chunkSize); // m_chunkSize to 0 means it is not initialised yet, so return the default value in this case
};

/**
 * Set the size, in bytes, of chunks in which the file is divided for encryption
 * This size must be a multiple of 16.
 * If the size is set on an existing file and differs from previous setting, an exception is generated
 * Default chunk size at file creation is 4kB
 */
void VfsEncryption::chunkSize_set(const size_t size) {
	if (size < 16 || size > 1048560) {
		throw EVFS_EXCEPTION<<"Encrypted VFS cannot set a chunk size "<<size<<" bytes. Acceptable range is [16, 1048560]";
	}
	// The chunk size MUST be a multiple of 16
	if (size%16 != 0) {
		throw EVFS_EXCEPTION<<"Encrypted VFS cannot set a chunk size "<<size<<" not multiple of 16";
	}

	// if chunk size is still a 0, we can set whatever value
	if (m_chunkSize == 0) {
		m_chunkSize = size;
	} else { // chunk size is already set for this file, we cannot change it, check the are the same
		if (m_chunkSize != size) {
			throw EVFS_EXCEPTION<<"Encrypted VFS to set chunk size "<<size<<" on file "<<m_filename<<" but already set to "<<m_chunkSize;
		}
	}
}

/**
 * Set a callback called during file opening to get the encryption material and suite
 */
void VfsEncryption::openCallback_set(EncryptedVfsOpenCb cb) noexcept {
	VfsEncryption::s_openCallback = cb;
}

/**
 * Get the callback called during file opening to get the encryption material and suite
 */
EncryptedVfsOpenCb VfsEncryption::openCallback_get() noexcept {
	return VfsEncryption::s_openCallback;
}

/**
 * Copy the secret material
 */
void VfsEncryption::secretMaterial_set(const std::vector<uint8_t> &secretMaterial) {
	if (m_module == nullptr) {
		throw EVFS_EXCEPTION << "Cannot set secret material before specifying which encryption suite to use. file "<<m_filename;
	}
	m_module->setModuleSecretMaterial(secretMaterial);
}

/**
 * Set an encryption suite
 */
void VfsEncryption::encryptionSuite_set(const EncryptionSuite suite) {
	if (m_module == nullptr && m_fileSize == 0) { // file creation
		m_module = make_VfsEncryptionModule(suite);
	} else { // file already exists (if m_filesize!=0 and m_module is nullptr, it is an existing plain file, the encryptionSuite_get would return plain)
		if (encryptionSuite_get() != suite) {
			throw EVFS_EXCEPTION << "Encryption suite for file "<<m_filename<<" is already set to "<<static_cast<uint16_t>(encryptionSuite_get())<<" but we're trying to set it to "<<static_cast<uint16_t>(suite);
		}
	}
}
/**
 * Get the encryption suite
 */
EncryptionSuite VfsEncryption::encryptionSuite_get() const noexcept {
	if (m_module != nullptr) {
		return m_module->getEncryptionSuite();
	}
	if (m_fileSize > 0) { // we have no suite but file exists -> it is plain
		return bctoolbox::EncryptionSuite::plain;
	}

	return EncryptionSuite::unset;
}

/* return the size of the raw file */
size_t VfsEncryption::r_fileSize() const noexcept {
	// first compute the number of chunks in our file
	size_t n = 0;
	// if we have an incomplete chunk
	if ((m_fileSize%m_chunkSize)>0) {
		n = 1;
	}
	n += m_fileSize/m_chunkSize;

	return m_fileSize // actual plain size
		+ n*m_module->getChunkHeaderSize() // all chunks' header size
		+ baseFileHeaderSize + m_headerExtensionSize + m_module->getModuleFileHeaderSize(); // file header size
}

/**
 * Get raw header: encryption module might check integrity on header
 * This function returns the raw header, without the encryption module part
 */
const std::vector<uint8_t>& VfsEncryption::r_getHeader() const noexcept {
	return r_header;
}

/**
 * Parse the file header
 * Header format for current version (1.00) is:
 *
 *    - magicnumber: hexadecimal ASCII for bcEncryptedFs: 0x6263456E637279707465644673 : 13 bytes
 *    - version Number: 0xMMmm : 2 bytes
 *    - encryption suite: 2 bytes
 *    - chunk size : 2 bytes : size of a chunk payload in number of 16 bytes blocks - excluding chunk header if any. -> maximum chunk size is 2^16-1*16 = 1 MB - recommended is 4Kb (256 blocks)
 *    - Header extension size: 2 bytes. Flexibility on header size - this version of parser may be able to read newer future versions
 *    - size: clear text file size : 8 bytes.
 *    - [Optionnal Header Extension - future versions of the vfs encryption]
 *    - [Optionnal Encryption module data - size is given by the encryption suite selected]
 *
 *
 * @throw a EVfsException is something goes wrong
 */
void VfsEncryption::parseHeader() {
	// check file exists
	int64_t ret = bctbx_file_size(pFileStd); // bctbx_file_size return a signed value...
	if (ret<baseFileHeaderSize) { // this is not an EVFS file, assume it is plain
		m_fileSize = ret; // set file size so we know that the file exists and is plain
		m_module = nullptr;
		return;
	}
	uint64_t fileSize = ret; // turn it into an unsigned one after checking it is >0.

	// read the header
	r_header = std::vector<uint8_t>(baseFileHeaderSize);
	size_t index=0;
	if (bctbx_file_read(pFileStd, r_header.data(), baseFileHeaderSize, 0) != baseFileHeaderSize) throw EVFS_EXCEPTION<<"parseHeader: unable to read encrypted vfs header";

	// check it starts with the magic number
	if (!std::equal(BCENCRYPTEDFS.cbegin(), BCENCRYPTEDFS.cend(), r_header.cbegin())) { // file is not starting with our magic number, assume it is plain
		m_fileSize = ret; // set file size so we know that the file exists and is plain
		m_module = nullptr;
		return;
	}
	index += BCENCRYPTEDFS.size();

	// check the version number
	m_versionNumber = r_header[index]<<8|r_header[index+1];
	if ( m_versionNumber != BcEncFS_v0100 ) {
		BCTBX_SLOGW<<"Encrypted FS trying to open a file version "<<m_versionNumber<<" but supports up to "<<BcEncFS_v0100<<", this may not work, proceed anyway";
	}
	index += 2;

	// get the encryption suite, check it is supported and instanciate the matching module
	uint16_t encryptionSuite = r_header[index]<<8|r_header[index+1];
	index += 2;

	// get chunk size, convert it in bytes
	m_chunkSize = (r_header[index]<<8|r_header[index+1])*16;
	index += 2;

	// check the header extension, not used in version 1.0 but use this field to skip them if any
	m_headerExtensionSize = r_header[index]<<8|r_header[index+1];
	index += 2;

	// get the file size
	m_fileSize = (static_cast<uint64_t>(r_header[index])<<56)
		| (static_cast<uint64_t>(r_header[index+1])<<48)
		| (static_cast<uint64_t>(r_header[index+2])<<40)
		| (static_cast<uint64_t>(r_header[index+3])<<32)
		| (static_cast<uint64_t>(r_header[index+4])<<24)
		| (static_cast<uint64_t>(r_header[index+5])<<16)
		| (static_cast<uint64_t>(r_header[index+6])<<8)
		| static_cast<uint64_t>(r_header[index+7]);


	// get the optional encryption scheme data if needed
	size_t encryptionModuleDataSize = moduleFileHeaderSize(encryptionSuite);

	// read the data, the are at offset baseFileHeaderSize + m_headerExtensionSize
	auto encryptionSuiteData = std::vector<uint8_t>(encryptionModuleDataSize);
	if (encryptionModuleDataSize != 0) {
		if (bctbx_file_read(pFileStd, encryptionSuiteData.data(), encryptionModuleDataSize, baseFileHeaderSize+m_headerExtensionSize) - encryptionModuleDataSize != 0) {
			throw EVFS_EXCEPTION<<"Encrypted FS: unable to read encryption scheme data in file header";
		}
	}

	// instanciate the encryption module
	m_module = make_VfsEncryptionModule(encryptionSuite, encryptionSuiteData);

	// check file size match what we have :
	// TODO: If they do not match, check all chunks integrity and update ? Recovery from failure between write and header update at last write/truncate
	if (r_fileSize() != fileSize) {
		throw EVFS_EXCEPTION<<"Encrypted FS: meta data file size "<<m_fileSize<<" and actual raw filesize "<<fileSize<<" do not match this value";
	}
}

void VfsEncryption::writeHeader() {
	if (m_module == nullptr) {
		throw EVFS_EXCEPTION<< "Encrypted VFS: cannot write file Header when no encryption module is selected";
	}
	std::vector<uint8_t> header{BCENCRYPTEDFS}; // starts with the magic number
	header.reserve(baseFileHeaderSize+m_module->getModuleFileHeaderSize());

	// add version number
	header.emplace_back(m_versionNumber>>8);
	header.emplace_back(m_versionNumber&0xFF);

	// add encryption suite
	uint16_t int_suite = static_cast<uint16_t>(m_module->getEncryptionSuite());
	header.emplace_back(int_suite>>8);
	header.emplace_back(int_suite&0xFF);

	// add chunk size (turn it into 16 bytes block number)
	header.emplace_back(((m_chunkSize/16)>>8)&0xFF);
	header.emplace_back((m_chunkSize/16)&0xFF);

	// add header extension size: none for now
	header.emplace_back(0x00);
	header.emplace_back(0x00);

	// add file size
	header.emplace_back((m_fileSize>>56)&0xFF);
	header.emplace_back((m_fileSize>>48)&0xFF);
	header.emplace_back((m_fileSize>>40)&0xFF);
	header.emplace_back((m_fileSize>>32)&0xFF);
	header.emplace_back((m_fileSize>>24)&0xFF);
	header.emplace_back((m_fileSize>>16)&0xFF);
	header.emplace_back((m_fileSize>>8)&0xFF);
	header.emplace_back(m_fileSize&0xFF);

	// update header cache (do not cache the encryption module data)
	// moduleFileHeader shall depends on the file header as it probably authentify it,
	// so do this update before asking for the encryption module header
	r_header = header;

	// add encryption module data
	auto moduleFileHeader = m_module->getModuleFileHeader(*this);
	header.insert(header.end(), moduleFileHeader.cbegin(), moduleFileHeader.cend());

	// write header to file
	ssize_t ret = bctbx_file_write(pFileStd, header.data(), header.size(), 0);
	if (ret - header.size() != 0) { // cannot compare directly signed and unsigned...
		throw EVFS_EXCEPTION<< "Encrypted VFS: something went wrong while writing file header. file_write returns "<<ret<<" but we expected "<< header.size();
	}
}

uint64_t VfsEncryption::fileSize_get() const noexcept {
	// plain file?
	if (m_module == nullptr) {
		return bctbx_file_size(pFileStd);
	}
	return m_fileSize;
}

/** return the size of a chunk including its encryption header */
size_t VfsEncryption::r_chunkSize() const noexcept {
	return m_chunkSize + m_module->getChunkHeaderSize();
};

/**
 * in which chunk is this offset?
 */
uint32_t VfsEncryption::getChunkIndex(off_t offset) const noexcept {
	return offset/m_chunkSize;
}

/**
 * @returns the offset, in the actual file, of the begining of the given chunk
 */
size_t VfsEncryption::getChunkOffset(uint32_t index) const noexcept {
	return r_chunkSize()*index // all previous chunks
		+ baseFileHeaderSize + m_headerExtensionSize + m_module->getModuleFileHeaderSize();
}

std::vector<uint8_t> VfsEncryption::read(size_t offset, size_t count) const {
	// plain file?
	if (m_module == nullptr) {
		std::vector<uint8_t> plain(count);
		auto readSize = bctbx_file_read(pFileStd, plain.data(), plain.size(), offset);
		plain.resize(readSize);
		return plain;
	}

	/* first compute how much of the actual file we must read */
	uint32_t firstChunk = getChunkIndex(offset);
	uint32_t lastChunk = getChunkIndex(offset+count-1); // -1 as we read data from indexes offset to offset + count - 1
	size_t offsetInFirstChunk = offset%m_chunkSize;

	// allocate a vector large enough to store all the data to read : number of chunks * size of raw chunk(payload+header)
	std::vector<uint8_t> rawData((lastChunk-firstChunk+1)*r_chunkSize());

	/* read all chunks from actual file */
	ssize_t readSize = bctbx_file_read(pFileStd, rawData.data(), rawData.size(), getChunkOffset(firstChunk));

	/* resize rawData to the actual content size - last chunk may be incomplete */
	if (readSize >= 0) {
		rawData.resize(readSize);
	} else {
		throw EVFS_EXCEPTION<<"fail to read file "<<m_filename<<" file_read returned "<<readSize;
	}

	std::vector<uint8_t> plainData{};
	plainData.reserve((lastChunk-firstChunk+1)*m_chunkSize);

	// decrypt everything we have chunk by chunk, use firstChunk as chunk index
	while (rawData.size() > m_module->getChunkHeaderSize()) {
		auto plainChunk = m_module->decryptChunk(firstChunk++, std::vector<uint8_t>(rawData.cbegin(), rawData.cbegin()+std::min(r_chunkSize(), rawData.size())));
		plainData.insert(plainData.end(), plainChunk.cbegin(), plainChunk.cend());
		// remove the decrypted chunk
		rawData.erase(rawData.begin(), rawData.begin()+std::min(r_chunkSize(), rawData.size()));
	}

	// return only the requested part
	plainData.erase(plainData.begin(), plainData.begin()+std::min(offsetInFirstChunk, plainData.size())); // remove unwanted chunk begining
	if (count<plainData.size()) { // if we have too much data, remove the end
		plainData.erase(plainData.begin()+count, plainData.end());
	}
	return plainData;
}

size_t VfsEncryption::write(const std::vector<uint8_t> &plainData, size_t offset) {
	// plain file?
	if (m_module == nullptr) {
		ssize_t ret = bctbx_file_write(pFileStd, plainData.data(), plainData.size(), offset);
		if ( ret - plainData.size() == 0) { // compare signed and unsigned
			return plainData.size();
		} else {
			throw EVFS_EXCEPTION<<"plain file fail to write to physical file "<< ret;
		}
	}

	auto plain = plainData; // work on a local copy as we may modify it
	size_t finalFileSize = std::max(m_fileSize, plain.size()+offset); // we might need to increase the file size

	// Are we writing after the end of the file, if yes, prepend with zeros
	if (offset > m_fileSize) {
		plain.insert(plain.begin(), offset-m_fileSize, 0);
		offset = m_fileSize;
	}

	uint32_t firstChunk = getChunkIndex(offset);
	uint32_t lastChunk = getChunkIndex(offset+plain.size()-1); // -1 as we write data from indexes offset to offset + data size - 1
	size_t rawDataSize = (lastChunk-firstChunk+1)*r_chunkSize(); // maximum size used, last chunk might be incomplete
	std::vector<uint8_t> rawData{}; // Store the existing encrypted chunks with header that are overwritten by this operation

	// Are we overwritting some chunks?
	size_t readOffset = offset - offset%m_chunkSize; // we must start read/write at the begining of a chunk
	if (readOffset<m_fileSize) { // Yes we are overwritting some data, read all the existing chunks we are overwritting
		rawData.resize(rawDataSize);
		ssize_t overwrittenSize = bctbx_file_read(pFileStd, rawData.data(), rawDataSize, getChunkOffset(firstChunk));
		rawData.resize(overwrittenSize);
		rawData.shrink_to_fit();
	}

	// prepend the plain buffer if needed
	if (readOffset<offset) { // we need to get the plain data from readOffset to offset
		// decrypt the first chunk
		auto plainChunk = m_module->decryptChunk(firstChunk, std::vector<uint8_t>(rawData.cbegin(), rawData.cbegin() + std::min(r_chunkSize(), rawData.size())));
		plain.insert(plain.begin(), plainChunk.cbegin(), plainChunk.cbegin()+offset-readOffset); // prepend the begining to our plain buffer
	}

	// append the plain buffer if needed:
	if ((plain.size()%m_chunkSize != 0) && (plain.size()+readOffset < m_fileSize)){ // We do not have an integer number of chunks to write and we have data after our last written byte
		auto plainChunk = m_module->decryptChunk(lastChunk, std::vector<uint8_t>(rawData.cbegin()+getChunkOffset(lastChunk)-getChunkOffset(firstChunk), rawData.cbegin()+std::min(getChunkOffset(lastChunk+1)-getChunkOffset(firstChunk), rawData.size())));
		plain.insert(plain.end(), plainChunk.cbegin()+(plain.size()%m_chunkSize), plainChunk.cend()); // append what is over the part we will write.
	}

	// encrypt the overwritten chunks
	std::vector<uint8_t> updatedRawData{};
	updatedRawData.reserve(rawDataSize);
	uint32_t currentChunkIndex = firstChunk;
	while (rawData.size()>0) {
		// get a chunk to re-encrypt
		std::vector<uint8_t> rawChunk(rawData.cbegin(), rawData.cbegin()+std::min(r_chunkSize(), rawData.size()));
		// delete it
		rawData.erase(rawData.begin(), rawData.begin()+std::min(r_chunkSize(), rawData.size()));
		// re-encrypt
		m_module->encryptChunk(currentChunkIndex++, rawChunk, std::vector<uint8_t>(plain.cbegin(), plain.cbegin()+std::min(m_chunkSize, plain.size())));
		// delete consumed plain
		plain.erase(plain.begin(), plain.begin()+std::min(m_chunkSize, plain.size()));
		// store the result
		updatedRawData.insert(updatedRawData.end(), rawChunk.cbegin(), rawChunk.cend());
	}

	// add new chunks if some data remains in the plain buffer
	while (plain.size()>0) {
		auto rawChunk = m_module->encryptChunk(currentChunkIndex++, std::vector<uint8_t>(plain.cbegin(), plain.cbegin()+std::min(m_chunkSize, plain.size())));
		// delete consumed plain
		plain.erase(plain.begin(), plain.begin()+std::min(m_chunkSize, plain.size()));
		// store the result
		updatedRawData.insert(updatedRawData.end(), rawChunk.cbegin(), rawChunk.cend());
	}

	// now actually write the rawData in the file
	ssize_t ret = bctbx_file_write(pFileStd, updatedRawData.data(), updatedRawData.size(), getChunkOffset(firstChunk));
	if ( ret - updatedRawData.size() == 0) { // compare signed and unsigned
		m_fileSize = finalFileSize;
		writeHeader();
		return plainData.size();
	} else {
		throw EVFS_EXCEPTION<<"fail to write to physical file "<<m_filename<<" file_write "<< ret;
	}
}


void VfsEncryption::truncate(const size_t newSize) {
	// plain file?
	if (m_module == nullptr) {
		bctbx_file_truncate(pFileStd, newSize);
		return;
	}

	// if current size is smaller, just write 0 at the end
	if (m_fileSize < newSize) {
		write(std::vector<uint8_t>{}, newSize); // write nothing at new size index, the gap is filled with 0 by write
		return;
	}

	if (m_fileSize > newSize) {
		// If the last chunk is modified, we must re-encrypt it
		if (newSize%m_chunkSize != 0) {
			// allocate a vector large enough to store a complete chunk
			std::vector<uint8_t> rawData(r_chunkSize());

			// read the future last chunk from actual file
			ssize_t readSize = bctbx_file_read(pFileStd, rawData.data(), rawData.size(), getChunkOffset(getChunkIndex(newSize)));
			rawData.resize(readSize);
			// decrypt it
			auto plainLastChunk = m_module->decryptChunk(getChunkIndex(newSize), std::vector<uint8_t>(rawData.cbegin(), rawData.cbegin()+std::min(r_chunkSize(), rawData.size())));
			// truncate the part we don't need anymore
			plainLastChunk.resize(newSize%m_chunkSize);
			// re-encrypt it
			m_module->encryptChunk(getChunkIndex(newSize), rawData, std::vector<uint8_t>(plainLastChunk.cbegin(), plainLastChunk.cend()));

			/* write it to the actual file */
			if (bctbx_file_write(pFileStd, rawData.data(), rawData.size(), getChunkOffset(getChunkIndex(newSize))) - rawData.size() != 0) {
				throw EVFS_EXCEPTION << "Cannot write file "<<m_filename<<" during truncate";
			}
		}
		// update file size in meta data
		m_fileSize = newSize;
		// truncate the actual file
		bctbx_file_truncate(pFileStd, r_fileSize());
		// update the header
		writeHeader();
	}
}

std::string VfsEncryption::filename_get() const noexcept {
	return m_filename;
}

/**
 * Opens the file with filename fName, associate it to the file handle pointed
 * by pFile, sets the methods bctbx_io_methods_t to the bcio structure
 * and initializes the file size.
 * Sets the error in pErrSvd if an error occurred while opening the file fName.
 * @param  pVfs    		Pointer to  bctx_vfs  VFS.
 * @param  fName   		Absolute path filename.
 * @param  openFlags    Flags to use when opening the file.
 * @return         		BCTBX_VFS_ERROR if an error occurs, BCTBX_VFS_OK otherwise.
 */
static  int bcOpen(bctbx_vfs_t *pVfs, bctbx_vfs_file_t *pFile, const char *fName, int openFlags);

bctbx_vfs_t bctoolbox::bcEncryptedVfs = {
	"bctbx_encrypted_vfs",               /* vfsName */
	bcOpen,						/*xOpen */
};

/**
 * Closes file by closing the associated file descriptor.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  pFile 	bctbx_vfs_file_t File handle pointer.
 * @return       	BCTBX_VFS_OK if successful, BCTBX_VFS_ERROR otherwise.
 */
static int bcClose(bctbx_vfs_file_t *pFile) {
	int ret = BCTBX_VFS_OK;
	if (pFile && pFile->pUserData) {
		VfsEncryption *ctx = static_cast<VfsEncryption *>(pFile->pUserData);
		delete(ctx); // that will close the file
		pFile->pUserData=NULL;
	}
	return ret;
}

/**
 * Simply sync the file contents given through the file handle
 * Just forward the request to underlying vfs
 */
static int bcSync(bctbx_vfs_file_t *pFile) {
	if (pFile && pFile->pUserData) {
		VfsEncryption *ctx = static_cast<VfsEncryption *>(pFile->pUserData);
		return bctbx_file_sync(ctx->pFileStd);
	}
	return BCTBX_VFS_ERROR;
}

/**
 * Read count bytes from the open file given by pFile, starting at offset.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurred.
 * @param  pFile  File handle pointer.
 * @param  buf    buffer to write the read bytes to.
 * @param  count  number of bytes to read
 * @param  offset file offset where to start reading
 * @return -errno if erroneous read, number of bytes read (count) on success,
 *                if the error was something else BCTBX_VFS_ERROR otherwise
 */
static ssize_t bcRead(bctbx_vfs_file_t *pFile, void *buf, size_t count, off_t offset) {
	if (pFile && pFile->pUserData) {
		VfsEncryption *ctx = static_cast<VfsEncryption *>(pFile->pUserData);

		try {
			auto readBuffer = ctx->read(offset, count);

			memcpy (buf, readBuffer.data(), readBuffer.size());
			return readBuffer.size();
		} catch (EVfsException const &e) { // cannot let raise an exception to a C context
			BCTBX_SLOGE<<"Encrypted VFS: error while reading "<<count<<" bytes from file "<<ctx->filename_get()<<" at offset "<<offset<<". "<<e;
		}
	}
	return BCTBX_VFS_ERROR;
}

/**
 * Writes directly to the open file given through the pFile argument.
 * Sets the error errno in the argument pErrSrvd after allocating it
 * if an error occurrred.
 * @param  p       bctbx_vfs_file_t File handle pointer.
 * @param  buf     Buffer containing data to write
 * @param  count   Size of data to write in bytes
 * @param  offset  File offset where to write to
 * @return         number of bytes written (can be 0), negative value errno if an error occurred.
 */
static ssize_t bcWrite(bctbx_vfs_file_t *pFile, const void *buf, size_t count, off_t offset) {
	if (pFile && pFile->pUserData) {
		VfsEncryption *ctx = static_cast<VfsEncryption *>(pFile->pUserData);
		return ctx->write(std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(buf), reinterpret_cast<const uint8_t *>(buf)+count), offset);
	}
	return BCTBX_VFS_ERROR;
}

/**
 * Returns the file size associated with the file handle pFile.
 * Return the size of cleartext file
 * @param pFile File handle pointer.
 * @return -errno if an error occurred, file size otherwise (can be 0).
 */
static int64_t bcFileSize(bctbx_vfs_file_t *pFile) {
	int ret = BCTBX_VFS_ERROR;
	if (pFile && pFile->pUserData) {
		VfsEncryption *ctx = static_cast<VfsEncryption *>(pFile->pUserData);
		return ctx->fileSize_get();
	}
	return ret;
}

/*
 ** Truncate a file
 * @param pFile File handle pointer.
 * @param new_size Extends the file with null bytes if it is superior to the file's size
 *                 truncates the file otherwise.
 * @return -errno if an error occurred, 0 otherwise.
  */
static int bcTruncate(bctbx_vfs_file_t *pFile, int64_t new_size){

	int ret = BCTBX_VFS_ERROR;
	if (pFile && pFile->pUserData) {
		VfsEncryption *ctx = static_cast<VfsEncryption *>(pFile->pUserData);
		ctx->truncate(new_size);
		return 0;
	}
	return ret;
}

static const  bctbx_io_methods_t bcio = {
	bcClose,		/* pFuncClose */
	bcRead,			/* pFuncRead */
	bcWrite,		/* pFuncWrite */
	bcTruncate,		/* pFuncTruncate */
	bcFileSize,		/* pFuncFileSize */
	bcSync,
	NULL // use the generic get next line function
};



static int bcOpen(bctbx_vfs_t *pVfs, bctbx_vfs_file_t *pFile, const char *fName, int openFlags) {
	VfsEncryption *ctx = nullptr;
	bctbx_vfs_file_t *stdFp = nullptr;
	try {
		if (pFile == NULL || fName == NULL) {
			return BCTBX_VFS_ERROR;
		}

		// encrypted vfs encapsulates the standard one, open the file with it
		// File cannot be writeonly as write operation may imply read/decrypt/write
		if ((openFlags&O_ACCMODE) == O_WRONLY) {
			openFlags &=~O_ACCMODE;
			openFlags |=O_RDWR;
		}

		stdFp = bctbx_file_open2(&bcStandardVfs, fName, openFlags);
		if (stdFp == NULL) return BCTBX_VFS_ERROR;

		pFile->pMethods = &bcio;

		ctx = new VfsEncryption(stdFp, fName);

		/* store the encryption context in the vfs UserData */
		pFile->pUserData = static_cast<void *>(ctx);
		return BCTBX_VFS_OK;

	} catch (EVfsException const &e) {// caller is most likely a C file(vfs.c), so swallow all exceptions
		if (stdFp != nullptr) {
			bctbx_file_close(stdFp);
		}
		delete(ctx);
		BCTBX_SLOGE<<"Encrypted VFS can't open File "<<fName<<" : "<< e;
		return BCTBX_VFS_ERROR;
	}
}
