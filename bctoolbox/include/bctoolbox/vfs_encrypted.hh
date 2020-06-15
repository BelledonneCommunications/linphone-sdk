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
#include "bctoolbox/exception.hh"
#include <functional>
#include <vector>
#include <memory>

namespace bctoolbox {

/**
 * @brief This dedicated exception inherits \ref BctoolboxException.
 *
 */
class EVfsException : public BctbxException {
public:
	EVfsException() = default;
	EVfsException(const std::string &message): BctbxException(message) {}
	EVfsException(const char *message): BctbxException(message) {}
	virtual ~EVfsException() throw() {}
	EVfsException(const EVfsException &other): BctbxException(other) {}

	template <typename T> EVfsException &operator<<(const T &val) {
		BctbxException::operator<<(val);
		return *this;
	}
};

#define EVFS_EXCEPTION EVfsException() << " " << __FILE__ << ":" << __LINE__ << " "


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
using EncryptedVfsOpenCb = std::function<void(VfsEncryption &settings)>;

// forward declare this type, store all the encryption data and functions
class VfsEncryptionModule;
/** Store in the bctbx_vfs_file_t userData field an object specific to encryption */
class VfsEncryption {
	friend class VfsEncryptionModule;
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
		uint16_t m_versionNumber; /**< version number of the encryption vfs */
		size_t m_chunkSize; /**< size of the file chunks payload in bytes : default is 4kB */
		size_t r_chunkSize() const noexcept; /** return the size of a chunk including its encryption header, as stored in the raw file */
		std::shared_ptr<VfsEncryptionModule> m_module; /**< one of the available encryption module */
		size_t m_headerExtensionSize; /**< header extension size */
		const std::string m_filename; /**< the filename as given to the open function */
		uint64_t m_fileSize; /**< size of the plaintext file */

		size_t r_fileSize() const noexcept; /**< return the size of the raw file */
		uint32_t getChunkIndex(off_t offset) const noexcept; /**< return the chunk index where to find the given offset */
		size_t getChunkOffset(uint32_t index) const noexcept; /**< return the offset in the actual file of the begining of the chunk */
		std::vector<uint8_t> r_header; /**< a cache of the header - without the encryption module data */

		/**
		 * Parse the header of an encrypted file, check everything seems correct
		 * may perform integrity checking if the encryption module provides it
		 *
		 * @throw a EVfsException if something goes wrong
		 */
		void parseHeader();
		/**
		 * Write the encrypted file header to the actual file
		 * Create the needed structures if the file is actually empty
		 *
		 * @throw a EVfsException if something goes wrong
		 **/
		void writeHeader();

	public:
		bctbx_vfs_file_t *pFileStd; /**< The encrypted vfs encapsulate a standard one */

		VfsEncryption(bctbx_vfs_file_t *stdFp, const std::string &filename);
		~VfsEncryption();


		/***
		 * Plain version of the file related accessors
		 ***/
		/**
		 * @return the size of the plain text file
		 */
		uint64_t fileSize_get() const noexcept;

		/* Read from file at given offset the requested size */
		std::vector<uint8_t> read(size_t offset, size_t count) const;

		/* write to file at given offset the requested size */
		size_t write(const std::vector<uint8_t> &plainData, size_t offset);

		/* Truncate the file to the given size, if given size is greater than current, pad with 0 */
		void truncate(const size_t size);

		/**
		 *  Get the filename
		 *  @return a string with the filename as given to the open function
		 */
		std::string filename_get() const noexcept;



		/***
		 * Encryption related API
		 ***/
		/**
		 * Set an encryption suite.
		 * When called at file creation, select the module to use for this file
		 * When called at the opening of an existing file, check it is the suite used at file creation, throw an exception if they differs
		 */
		void encryptionSuite_set(const EncryptionSuite);

		/**
		 * Set the secret Material in the encryption module
		 * This function cannot be called if a encryption suite was not set.
		 */
		void secretMaterial_set(const std::vector<uint8_t> &secretMaterial);

		/**
		 * Returns the encryption suite used for this file
		 * Can be return unset if the file is being created
		 */
		EncryptionSuite encryptionSuite_get() const noexcept;

		/**
		 * Returns the size of chunks in which the file is divided for encryption
		 */
		size_t chunkSize_get() const noexcept;
		/**
		 * Set the size, in bytes, of chunks in which the file is divided for encryption
		 * This size must be a multiple of 16, accepted values in range [16, (2^16-1)*16].
		 * If the size is set on an existing file and differs from previous setting, an exception is generated
		 * Default chunk size at file creation is 4kB.
		 * A file holds a maximum of 2^32-1 chunks. 16 bytes chunks - not recommended smallest admissible value - limit the file size to 64GB
		 */
		void chunkSize_set(const size_t size);

		/**
		 * Get raw header: encryption module might check integrity on header
		 * This function returns the raw header, without the encryption module part
		 */
		const std::vector<uint8_t>& r_getHeader() const noexcept;


};


} // namespace bctoolbox
#endif /* BCTBX_VFS_STANDARD_HH */
