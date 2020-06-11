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
#include "bctoolbox/vfs_standard.h"
#include "bctoolbox/port.h"
#include "bctoolbox/logging.h"
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>

using namespace bctoolbox;

std::shared_ptr<VfsEncryptionModule> bctoolbox::make_VfsEncryptionModule(const EncryptionSuite suite) {
	switch (suite) {
		case EncryptionSuite::dummy:
			return std::make_shared<VfsEncryptionModuleDummy>();
			break;
		case EncryptionSuite::unset:
			return nullptr;
	}
	return nullptr;
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


/**
 * Initialiase the static callback property
 */
EncryptedVfsOpenCb VfsEncryption::s_openCallback = nullptr;

VfsEncryption::VfsEncryption(bctbx_vfs_file_t *stdFp) noexcept : pFileStd(stdFp) {

	/* initialise it */
	m_fOffset = 0;
	m_versionNumber = BcEncFS_v0100; // default version number is the last available
	m_chunkSize = 16;//4096; // MUST be a multiple of 16 bytes - default : 256 blocks of 16 bytes : 4KB. TODO: make it settable
	m_module = nullptr;
	m_encryptionSuiteData = std::vector<uint8_t>{};
	m_secretMaterial = std::vector<uint8_t>{};
	m_headerExtensionSize = 0;
	m_headerSize = 0;
	m_fileSize = 0;
}

VfsEncryption::~VfsEncryption() {
	BCTBX_SLOGD<<"JOHAN: closing file ";
	if (pFileStd != nullptr) {
		//TODO: try/catch all any exception raising from close and swallow it
		bctbx_file_close(pFileStd);
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
void VfsEncryption::secretMaterial_set(const std::vector<uint8_t> &secretMaterial) noexcept {
	m_secretMaterial = secretMaterial;
}

/**
 * Set an encryption suite
 */
void VfsEncryption::encryptionSuite_set(const EncryptionSuite suite) noexcept {
	if (m_module == nullptr) {
		m_module = make_VfsEncryptionModule(suite);
	} else {
		if (m_module->getEncryptionSuite() != suite) {
			// TODO: throw an exception, we cannot change the encryption suite this way (or shall we?)
		}
	}
}
/**
 * Get the encryption suite
 */
EncryptionSuite VfsEncryption::encryptionSuite_get() noexcept {
	if (m_module != nullptr) {
		return m_module->getEncryptionSuite();
	}
	return EncryptionSuite::unset;
}

/* return the number of file chunks */
size_t VfsEncryption::getChunksNb() noexcept {
	size_t n = 0;
	// if we have an incomplete chunk
	if ((m_fileSize%m_chunkSize)>0) {
		n = 1;
	}
	n += m_fileSize/m_chunkSize;
	return n;
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
 * @return BCTBX_VFS_ERROR or BCTBX_VFS_OK
 */
int VfsEncryption::parseHeader() noexcept {
	/* check file exists*/
	if (pFileStd == nullptr) return BCTBX_VFS_ERROR;
	int64_t ret = bctbx_file_size(pFileStd); // bctbx_file_size return a signed value...
	if (ret<baseFileHeaderSize && ret!=0) return BCTBX_VFS_ERROR;
	uint64_t fileSize = ret; // turn it into an unsigned one after checking it is >0.

	if (fileSize==0) return BCTBX_VFS_OK; // nothing to parse...

	/* read the header */
	uint8_t headerBuffer[baseFileHeaderSize];
	size_t index=0;
	if (bctbx_file_read(pFileStd, headerBuffer, baseFileHeaderSize, 0) != baseFileHeaderSize) return BCTBX_VFS_ERROR;

	/* check it starts with the magic number */
	if (memcmp(BCENCRYPTEDFS.data(), headerBuffer, BCENCRYPTEDFS.size()) != 0) return BCTBX_VFS_ERROR;
	index += BCENCRYPTEDFS.size();

	/* check the version number */
	m_versionNumber = headerBuffer[index]<<8|headerBuffer[index+1];
	if ( m_versionNumber != BcEncFS_v0100 ) {
		bctbx_warning("Encrypted FS trying to open a file version %0x, but supports up to %0x, this may not work, proceed anyway", m_versionNumber, BcEncFS_v0100);
	}
	index += 2;

	/* get the encryption suite, check it is supported and instanciate the matching module */
	uint16_t encryptionSuite = headerBuffer[index]<<8|headerBuffer[index+1];
	switch (encryptionSuite) {
		/* all supported scheme must be listed here */
		case static_cast<uint16_t>(EncryptionSuite::dummy):
			m_module = make_VfsEncryptionModule(EncryptionSuite::dummy);
			if (m_module == nullptr) return BCTBX_VFS_ERROR;
			break;
		case static_cast<uint16_t>(EncryptionSuite::unset):
		default:
			bctbx_error("Encrypted FS: unsupported encryption scheme %04x", encryptionSuite);
			return BCTBX_VFS_ERROR;
	}
	index += 2;

	/* get chunk size, convert it in bytes */
	m_chunkSize = (headerBuffer[index]<<8|headerBuffer[index+1])*16;
	index += 2;

	/* check the header extension, not used in version 1.0 but use this field to skip them if any */
	m_headerExtensionSize = headerBuffer[index]<<8|headerBuffer[index+1];
	BCTBX_SLOGD<<"JOHAN PARSE HEADER. Set headerExtensionSize to "<<m_headerExtensionSize;
	index += 2;

	/* get the file size */
	m_fileSize = (static_cast<uint64_t>(headerBuffer[index+0])<<56)
		| (static_cast<uint64_t>(headerBuffer[index+1])<<48)
		| (static_cast<uint64_t>(headerBuffer[index+2])<<40)
		| (static_cast<uint64_t>(headerBuffer[index+3])<<32)
		| (static_cast<uint64_t>(headerBuffer[index+4])<<24)
		| (static_cast<uint64_t>(headerBuffer[index+5])<<16)
		| (static_cast<uint64_t>(headerBuffer[index+6])<<8)
		| static_cast<uint64_t>(headerBuffer[index+7]);

	/* get the optional encryption scheme data if needed */
	size_t encryptionModuleDataSize = m_module->getModuleFileHeaderSize();

	/* check file size match what we have :
	 * - file header(baseFileHeader + headerExtension + moduleData + nb_chunks*module chunk header size)
	 * - content
	 */
	if (m_fileSize + baseFileHeaderSize + m_headerExtensionSize + encryptionModuleDataSize + getChunksNb()*m_module->getChunkHeaderSize() != fileSize) {
		bctbx_error("Encrypted FS: meta data file size (%ld) and actual filesize do not match", m_fileSize);
		return BCTBX_VFS_ERROR;
	}

	/* read the data, the are at offset baseFileHeaderSize + m_headerExtensionSize */
	if (encryptionModuleDataSize != 0) {
		m_encryptionSuiteData = std::vector<uint8_t>(encryptionModuleDataSize);
		if (bctbx_file_read(pFileStd, m_encryptionSuiteData.data(), encryptionModuleDataSize, baseFileHeaderSize+m_headerExtensionSize) - encryptionModuleDataSize != 0) {
			bctbx_error("Encrypted FS: unable to read encryption scheme data in file header");
			return BCTBX_VFS_ERROR;
		}
	}




	/* TODO: encryption scheme shall provide a integrity check -> if yes verify it here */

	return BCTBX_VFS_OK;
}

int VfsEncryption::writeHeader() noexcept {
	if (m_module == nullptr) {
		BCTBX_SLOGE<<"Encrypted VFS: cannot write file Header when no encryption module is selected";
		return BCTBX_VFS_ERROR;
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

	// add encryption module data
	auto moduleFileHeader = m_module->getModuleFileHeader();
	header.insert(header.end(), moduleFileHeader.cbegin(), moduleFileHeader.cend());

	// write header to file
	ssize_t ret = bctbx_file_write(pFileStd, header.data(), header.size(), 0);

	if (ret-header.size() != 0) { // cannot compare directly signed and unsigned...
		return BCTBX_VFS_ERROR;
	}

	return BCTBX_VFS_OK;
}

uint64_t VfsEncryption::fileSize_get() noexcept {
	return m_fileSize;
}

off_t VfsEncryption::fOffset_get() noexcept {
	return m_fOffset;
}
void VfsEncryption::fOffset_set(const off_t offset) noexcept {
	m_fOffset=offset;
}
/** return the size of a chunk including its encryption header */
size_t VfsEncryption::r_chunkSize() const noexcept {
	return m_chunkSize + m_module->getChunkHeaderSize();
};

/**
 * in which chunk is this offset?
 */
uint32_t VfsEncryption::getChunkIndex(off_t offset) noexcept {
	return offset/m_chunkSize;
}

/**
 * @returns the offset, in the actual file, of the begining of the given chunk
 */
size_t VfsEncryption::getChunkOffset(uint32_t index) noexcept {
	return r_chunkSize()*index // all previous chunks
		+ baseFileHeaderSize + m_headerExtensionSize + m_module->getModuleFileHeaderSize();
}

std::vector<uint8_t> VfsEncryption::read(size_t offset, size_t count) {
	/* first compute how much of the actual file we must read */
	uint32_t firstChunk = getChunkIndex(offset);
	uint32_t lastChunk = getChunkIndex(offset+count-1); // -1 as we read data from indexes offset to offset + count - 1
	size_t offsetInFirstChunk = offset%m_chunkSize;
	BCTBX_SLOGD<<"JOHAN read: "<<count<<" bytes at offset "<<offset;

	// allocate a vector large enough to store all the data to read : number of chunks * size of raw chunk(payload+header)
	std::vector<uint8_t> rawData((lastChunk-firstChunk+1)*r_chunkSize());

	/* read all chunks from actual file */
	ssize_t readSize = bctbx_file_read(pFileStd, rawData.data(), rawData.size(), getChunkOffset(firstChunk));

	/* resize rawData to the actual content size - last chunk may be incomplete */
	if (readSize >= 0) {
		rawData.resize(readSize);
	} else {
		// throw an exception: read gone wrong
	}

	std::vector<uint8_t> plainData{};
	plainData.reserve((lastChunk-firstChunk+1)*m_chunkSize);

	// decrypt everything we have chunk by chunk
	while (rawData.size() > m_module->getChunkHeaderSize()) {
		auto plainChunk = m_module->decryptChunk(std::vector<uint8_t>(rawData.cbegin(), rawData.cbegin()+std::min(r_chunkSize(), rawData.size())));
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
	auto plain = plainData; // work on a local copy as we may modify it
	size_t finalFileSize = std::max(m_fileSize, plain.size()+offset); // we might need to increase the file size

	// Are we writing after the end of the file, if yes, prepend with zeros
	if (offset > m_fileSize) {
		plain.insert(plain.begin(), offset-m_fileSize, 0);
		offset = m_fileSize;
	}

	BCTBX_SLOGD<<"JOHAN write : offset is "<<offset<<"size is "<<plain.size();
	uint32_t firstChunk = getChunkIndex(offset);
	uint32_t lastChunk = getChunkIndex(offset+plain.size()-1); // -1 as we write data from indexes offset to offset + data size - 1
	BCTBX_SLOGD<<"JOHAN write : first chunk index "<<firstChunk<<"last chunk index "<<lastChunk;
	size_t rawDataSize = (lastChunk-firstChunk+1)*r_chunkSize(); // maximum size used, last chunk might be incomplete
	std::vector<uint8_t> rawData{}; // Store the existing encrypted chunks with header that are overwritten by this operation

	// Are we overwritting some chunks?
	size_t readOffset = offset - offset%m_chunkSize; // we must start read/write at the begining of a chunk
	if (readOffset<m_fileSize) { // Yes we are overwritting some data, read all the existing chunks we are overwritting
		BCTBX_SLOGD<<"JOHAN write : we are overwritting, get raw data from raw index "<<getChunkOffset(firstChunk)<<" get "<< rawDataSize<<" bytes";
		rawData.resize(rawDataSize);
		ssize_t overwrittenSize = bctbx_file_read(pFileStd, rawData.data(), rawDataSize, getChunkOffset(firstChunk));
		rawData.resize(overwrittenSize);
		BCTBX_SLOGD<<"JOHAN write : we are overwritting, get raw data from raw index "<<getChunkOffset(firstChunk)<<" got "<< overwrittenSize<<" bytes"<<std::endl<<"    "<<getHex(rawData);
		rawData.shrink_to_fit();
	}

	// prepend the plain buffer if needed
	if (readOffset<offset) { // we need to get the plain data from readOffset to offset
		// decrypt the first chunk
		auto plainChunk = m_module->decryptChunk(std::vector<uint8_t>(rawData.cbegin(), rawData.cbegin() + std::min(r_chunkSize(), rawData.size())));
		plain.insert(plain.begin(), plainChunk.cbegin(), plainChunk.cbegin()+offset-readOffset); // prepend the begining to our plain buffer
	}

	// append the plain buffer if needed:
	if ((plain.size()%m_chunkSize != 0) && (plain.size()+readOffset < m_fileSize)){ // We do not have an integer number of chunks to write and we have data after our last written byte
		auto plainChunk = m_module->decryptChunk(std::vector<uint8_t>(rawData.cbegin()+getChunkOffset(lastChunk)-getChunkOffset(firstChunk), rawData.cbegin()+std::min(getChunkOffset(lastChunk+1)-getChunkOffset(firstChunk), rawData.size())));
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
		m_module->encryptChunk(rawChunk, std::vector<uint8_t>(plain.cbegin(), plain.cbegin()+std::min(m_chunkSize, plain.size())));
		// delete consumed plain
		plain.erase(plain.begin(), plain.begin()+std::min(m_chunkSize, plain.size()));
		// store the result
		updatedRawData.insert(updatedRawData.end(), rawChunk.cbegin(), rawChunk.cend());
		// increase the current chunk index
		currentChunkIndex++;
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
	BCTBX_SLOGD<<"JOHAN: write file, size is "<< updatedRawData.size() << "bytes first Chunk is "<<firstChunk;
	BCTBX_SLOGD<<"JOHAN offset "<< getChunkOffset(firstChunk);
	if (bctbx_file_write(pFileStd, updatedRawData.data(), updatedRawData.size(), getChunkOffset(firstChunk)) - updatedRawData.size() == 0) {
		m_fileSize = finalFileSize;
		writeHeader();
		return plainData.size();
	} else {
		BCTBX_SLOGE<<"JOHAN write failure";
		//TODO error message, exception ?
		return 0;
	}
}


void VfsEncryption::truncate(const size_t newSize) {
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
			auto plainLastChunk = m_module->decryptChunk(std::vector<uint8_t>(rawData.cbegin(), rawData.cbegin()+std::min(r_chunkSize(), rawData.size())));
			// truncate the part we don't need anymore
			plainLastChunk.resize(newSize%m_chunkSize);
			// re-encrypt it
			m_module->encryptChunk(rawData, std::vector<uint8_t>(plainLastChunk.cbegin(), plainLastChunk.cend()));

			/* write it to the actual file */
			if (bctbx_file_write(pFileStd, rawData.data(), rawData.size(), getChunkOffset(getChunkIndex(newSize))) - rawData.size() != 0) {
				// TODO: generate an exception
			}
		}
		// update file size
		m_fileSize = newSize;
		// truncate the actual file
		bctbx_file_truncate(pFileStd, m_fileSize + baseFileHeaderSize + m_headerExtensionSize + m_module->getModuleFileHeaderSize() + getChunksNb()*m_module->getChunkHeaderSize());
		// update the header
		writeHeader();
	}
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
		delete(ctx);
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

		// TODO : catch exception if errno from lower level
		auto readBuffer = ctx->read(offset, count);

		memcpy (buf, readBuffer.data(), readBuffer.size());
		return readBuffer.size();
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
	if (pFile == NULL || fName == NULL) {
		return BCTBX_VFS_ERROR;
	}

	/* encrypted vfs encapsulates the standard one, open the file with it */
	bctbx_vfs_file_t *stdFp = bctbx_file_open2(&bcStandardVfs, fName, openFlags);
	if (stdFp == NULL) return BCTBX_VFS_ERROR;

	pFile->pMethods = &bcio;

	auto ctx = new VfsEncryption(stdFp);

	/* If the file exists, read the header to check it is an encrypted file and gets its encrypted policy */
	bool createFile = ((openFlags&O_CREAT)!=0);
	if (bctbx_file_size(stdFp) >= baseFileHeaderSize) {
		if (ctx->parseHeader() == BCTBX_VFS_ERROR)  {
			delete(ctx);
			bctbx_error("File %s exists but is not a valid encrypted file", fName);
			return BCTBX_VFS_ERROR;
		}
		createFile = false;
	}

	/* if the static callback is set, call it */
	if (VfsEncryption::openCallback_get() != nullptr) {
		(VfsEncryption::openCallback_get())(fName, *ctx);
	}

	if (createFile) {
		if (ctx->writeHeader() == BCTBX_VFS_ERROR)  {
			delete(ctx);
			bctbx_error("File %s : unable to write encryption header", fName);
			return BCTBX_VFS_ERROR;
		}
	}

	/* store the encryption context in the vfs UserData */
	pFile->pUserData = static_cast<void *>(ctx);
	return BCTBX_VFS_OK;
}
