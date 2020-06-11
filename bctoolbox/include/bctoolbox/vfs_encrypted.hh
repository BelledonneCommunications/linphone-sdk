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

#ifndef BCTBX_VFS_ENCRYPTED_HH
#define BCTBX_VFS_ENCRYPTED_HH

#include "bctoolbox/vfs.h"
#include <functional>
#include <vector>
#include <memory>

namespace bctoolbox {
/**
 * Virtual File sytem provided
 */
extern bctbx_vfs_t bcEncryptedVfs;


/**
 * Provided encryption suites
 */
enum class EncryptionSuite : uint16_t {
	unset = 0,/**< no encryption suite selected */
	dummy = 1 /**< a test suite, do not use other than for test */
};

/* complete declaration follows, we need this one to define the callback type */
class VfsEncryption;

/**
 * Define a function prototype to be called at each file opening.
 * This function is a static class property, used to retrieve secretMaterial to encrypt/decrypt the file
 */
using EncryptedVfsOpenCb = std::function<void(const std::string &filename, VfsEncryption &settings)>;

// forward declare this type, store all the encryption data and functions
class VfsEncryptionModule;
/** Store in the bctbx_vfs_file_t userData field an object specific to encryption */
class VfsEncryption {
	/* Class properties and method */
	private:
		static EncryptedVfsOpenCb s_openCallback; /**< a class callback to get secret material at file opening. Implemented as static as it is called by constructor */
	public:
		/**
		 * at file opening a callback ask for crypto material, it is class property, set it using this class method
		 */
		static void openCallback_set(EncryptedVfsOpenCb cb) noexcept;
		static EncryptedVfsOpenCb openCallback_get() noexcept;

	/* Object properties and methods */
	private:
		off_t m_fOffset; /**< current read/write pointer */

		uint16_t m_versionNumber; /**< version number of the encryption vfs */
		size_t m_chunkSize; /**< size of the file chunks payload in bytes : default is 4kB */
		size_t r_chunkSize() const noexcept; /** return the size of a chunk including its encryption header */
		std::shared_ptr<VfsEncryptionModule> m_module; /**< one of the available encryption module */
		std::vector<uint8_t> m_encryptionSuiteData; /**< header encryption suite data - a cache of file global data related to encryption */
		std::vector<uint8_t> m_secretMaterial; /**< a buffer to store all the secrets needed by the encryption suite - this shall hold the master key for this file and is never copied in the file */
		size_t m_headerExtensionSize; /**< header extension size */
		size_t m_headerSize; /**< size of the file header */
		uint64_t m_fileSize; /**< size of the plaintext file */

		size_t getChunksNb() noexcept; /**< return the number of chunks in the file */
		uint32_t getChunkIndex(off_t offset) noexcept; /**< return the chunk index where to find the given offset */
		size_t getChunkOffset(uint32_t index) noexcept; /**< return the offset in the actual file of the begining of the chunk */


	public:
		bctbx_vfs_file_t *pFileStd; /**< The encrypted vfs encapsulate a standard one */

		VfsEncryption(bctbx_vfs_file_t *stdFp) noexcept;
		~VfsEncryption();
		void secretMaterial_set(const std::vector<uint8_t> &secretMaterial) noexcept;
		void encryptionSuite_set(const EncryptionSuite) noexcept;
		EncryptionSuite encryptionSuite_get() noexcept;


		/***
		 * Plain version of the file related accessors
		 ***/
		/**
		 * @return the size of the plain text file
		 */
		uint64_t fileSize_get() noexcept;
		/**
		 * @return the current read/write offset on plain text file
		 */
		off_t fOffset_get() noexcept;
		/**
		 * Set current read/write offset on the plain text file
		 * offset can be larger than the current fileSize. If it is the case, and some data is written, the gap is filled with 0(in plain version).
		 */
		void fOffset_set(const off_t offset) noexcept;

		/* Read from file at given offset the requested size */
		std::vector<uint8_t> read(size_t offset, size_t count);

		/* write to file at given offset the requested size */
		size_t write(const std::vector<uint8_t> &plainData, size_t offset);

		/* Truncate the file to the given size, if given size is greater than current, pad with 0 */
		void truncate(const size_t size);



		/***
		 * Encryption related API
		 ***/
		/**
		 * Parse the header of an encrypted file, check everything seems correct
		 * may perform integrity checking if the encryption module provides it
		 *
		 * @return BCTBX_VFS_OK or BCTBX_VFS_ERROR
		 */
		int parseHeader() noexcept;
		/**
		 * Write the encrypted file header to the actual file
		 * Create the needed structures if the file is actually empty
		 *
		 * @return BCTBX_VFS_OK or BCTBX_VFS_ERROR
		 **/
		int writeHeader() noexcept;


};


} // namespace bctoolbox
#endif /* BCTBX_VFS_STANDARD_HH */
