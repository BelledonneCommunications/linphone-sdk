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

#ifndef BCTBX_VFS_ENCRYPTION_MODULE_HH
#define BCTBX_VFS_ENCRYPTION_MODULE_HH

#include "bctoolbox/vfs_encrypted.hh"

namespace bctoolbox {
/**
 * Define the interface any encryption suite must provide
 */
class VfsEncryptionModule {
public:
	/**
	 * @return the size in bytes of the chunk header
	 */
	virtual size_t getChunkHeaderSize() const noexcept = 0;
	/**
	 * @return the size in bytes of file header module data
	 */
	virtual size_t getModuleFileHeaderSize() const noexcept = 0;
	/**
	 * @return the encryptionSuite implemented by the module
	 */
	virtual EncryptionSuite getEncryptionSuite() const noexcept = 0;

	/**
	 * Get from the module the data to store in the file header
	 */
	virtual const std::vector<uint8_t> getModuleFileHeader(const VfsEncryption &fileContext) const = 0;

	/**
	 * Set in the module, the secret material used for encryption
	 */
	virtual void setModuleSecretMaterial(const std::vector<uint8_t> &secret) = 0;

	/**
	 * Get the size of the secret material needed by this module
	 */
	virtual size_t getSecretMaterialSize() const noexcept = 0;

	/**
	 * Decrypt a data chunk
	 * @param[in] a vector which size shall be chunkHeaderSize + chunkSize holding the raw data read from disk
	 * @return the decrypted data chunk
	 */
	virtual std::vector<uint8_t> decryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &rawChunk) = 0;

	/**
	 * ReEncrypt a data chunk
	 * @param[in/out] rawChunk	The existing encrypted chunk
	 * @param[in]     plainData	The plain text to be encrypted
	 */
	virtual void
	encryptChunk(const uint32_t chunkIndex, std::vector<uint8_t> &rawChunk, const std::vector<uint8_t> &plainData) = 0;
	/**
	 * Encrypt a new data chunk
	 * @param[in]	chunkIndex	The chunk index
	 * @param[in]	plainData	The plain text to be encrypted
	 * @return the encrypted chunk
	 */
	virtual std::vector<uint8_t> encryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &plainData) = 0;

	/**
	 * Check the integrity over the whole file
	 * @param[in]	fileContext 	a way to access the file content
	 *
	 * @return 	true if the integrity check successfully passed, false otherwise
	 */
	virtual bool checkIntegrity(const VfsEncryption &fileContext) = 0;

	virtual ~VfsEncryptionModule(){};
};

} // namespace bctoolbox
#endif // BCTBX_VFS_ENCRYPTION_MODULE_HH
