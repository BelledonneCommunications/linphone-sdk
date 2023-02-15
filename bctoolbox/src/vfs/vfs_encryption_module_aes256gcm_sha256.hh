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

#ifndef BCTBX_VFS_ENCRYPTION_MODULE_AES256GCM_SHA256_HH
#define BCTBX_VFS_ENCRYPTION_MODULE_AES256GCM_SHA256_HH
#include "bctoolbox/crypto.hh"
#include "bctoolbox/vfs_encrypted.hh"
#include "vfs_encryption_module.hh"
#include <array>

/*********** The AES256-GCM SHA256 module   ************************
 * Key derivations:
 *    - file Header HMAC key = HKDF(Mk, fileHeaderSalt, "EVFS file header")
 *    - chunk encryption key = HKDF(Mk, fileHeaderSalt || Chunk Index, "EVFS chunk")
 * File Header:
 *    - 32 bytes auth tag: HMAC-sha256 on the file header
 *    - 16 bytes salt: random generated at file creation : input of the HKDF keyed by the master key.
 * Chunk Header:
 *    - Authentication tag : 16 bytes
 *    - IV: 12 bytes. A random updated at each encryption
 * Chunk encryption:
 *    - AES256-GCM with 128 bit auth tag. No associated Data.
 *    - IV is 12 bytes random : MUST use a random for IV as attacker having access to file system could restore an old
 * version of the file and monitor further writing. So deterministic IV could lead to key/IV reuse.
 */
namespace bctoolbox {
class VfsEM_AES256GCM_SHA256 : public VfsEncryptionModule {
private:
	/**
	 * The local RNG
	 */
	std::shared_ptr<bctoolbox::RNG> mRNG; // list it first so it is available in the constructor's init list

	/**
	 * File header
	 */
	std::vector<uint8_t> mFileSalt;
	std::array<uint8_t, SHA256::ssize()> mFileHeaderIntegrity;

	/** keys
	 */
	std::vector<uint8_t> sMasterKey;         // used to derive all keys
	std::vector<uint8_t> sFileHeaderHMACKey; // used to feed HMAC integrity check on file header

	/**
	 * Derive the key from master key for the given chunkIndex:
	 * HKDF(fileSalt || ChunkIndex, master Key, "EVFS chunk")
	 *
	 * @param[in]	chunkIndex	the chunk index used in key derivation
	 *
	 * @return	the AES256-GCM128 key
	 */
	std::vector<uint8_t> deriveChunkKey(uint32_t chunkIndex);

public:
	/**
	 * This function exists as static and non static
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
		return EncryptionSuite::aes256gcm128_sha256;
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
	VfsEM_AES256GCM_SHA256();
	// Opening an existing file
	VfsEM_AES256GCM_SHA256(const std::vector<uint8_t> &fileHeader);

	~VfsEM_AES256GCM_SHA256();
};

} // namespace bctoolbox
#endif // BCTBX_VFS_ENCRYPTION_MODULE_AES256GCM_SHA256_HH
