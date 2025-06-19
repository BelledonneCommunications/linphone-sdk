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

#ifndef BCTBX_VFS_ENCRYPTION_MODULE_DUMMY_HH
#define BCTBX_VFS_ENCRYPTION_MODULE_DUMMY_HH
#include "bctoolbox/vfs_encrypted.hh"
#include "vfs_encryption_module.hh"

namespace bctoolbox {
class VfsEncryptionModuleDummy : public VfsEncryptionModule {
private:
	/**
	 * Store the file header and secret
	 */
	std::vector<uint8_t> mFileHeader;
	std::vector<uint8_t> mFileHeaderIntegrity;
	std::vector<uint8_t> mSecret;

	/**
	 * Compute the integrity tag in the given chunk
	 */
	std::vector<uint8_t> chunkIntegrityTag(const std::vector<uint8_t> &chunk) const;

	/**
	 * Get the chunk index from the given chunk
	 */
	uint32_t getChunkIndex(const std::vector<uint8_t> &chunk) const;

	/**
	 * Get global IV. Part of IV common to all chunks
	 * The last 8 bytes of the file header
	 */
	std::vector<uint8_t> globalIV() const;

public:
	/**
	 * @return the size in bytes of file header module data
	 */
	static size_t moduleFileHeaderSize() noexcept;

	/**
	 * @return the size in bytes of the chunk header
	 */
	size_t getChunkHeaderSize() const noexcept override;

	/**
	 * @return the size in bytes of file header module data
	 */
	size_t getModuleFileHeaderSize() const noexcept override;

	/**
	 * @return the EncryptionSuite provided by this module
	 */
	EncryptionSuite getEncryptionSuite() const noexcept override {
		return EncryptionSuite::dummy;
	}

	/**
	 * @return the secret material size
	 */
	size_t getSecretMaterialSize() const noexcept override;

	/**
	 * Decrypt a chunk of data
	 * @param[in] a vector which size shall be chunkHeaderSize + chunkSize holding the raw data read from disk
	 * @return the decrypted data chunk
	 */
	std::vector<uint8_t> decryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &rawChunk) override;

	void encryptChunk(const uint32_t chunkIndex,
	                  std::vector<uint8_t> &rawChunk,
	                  const std::vector<uint8_t> &plainData) override;
	std::vector<uint8_t> encryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &plainData) override;

	const std::vector<uint8_t> getModuleFileHeader(const VfsEncryption &fileContext) const override;

	void setModuleSecretMaterial(const std::vector<uint8_t> &secret) override;

	/**
	 * Check the integrity over the whole file
	 * @param[in]	fileContext 	a way to access the file content
	 *
	 * @return 	true if the integrity check successfully passed, false otherwise
	 */
	bool checkIntegrity(const VfsEncryption &fileContext) override;

	/**
	 * constructors
	 */
	// At file creation
	VfsEncryptionModuleDummy();
	// Opening an existing file
	VfsEncryptionModuleDummy(const std::vector<uint8_t> &moduleFileHeader);

	~VfsEncryptionModuleDummy(){};
};

} // namespace bctoolbox
#endif // BCTBX_VFS_ENCRYPTION_MODULE_DUMMY
