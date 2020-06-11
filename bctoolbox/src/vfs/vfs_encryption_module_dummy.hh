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
/**
 * Define the interface any encryption suite must provide
 */
/* implementation note static polymorphism using recurring template */
class VfsEncryptionModuleDummy : public VfsEncryptionModule {
	private:
		/**
		 * Constant associated to this encryption module
		 */

		/** Header in the dummy module holds:
		 * - Block Index : 4 bytes. (-> max 4 giga blocks in a file -> more than enough)
		 * - Encryption Counter : 4 bytes counter (increased at each encryption)
		 * Total size : 8 bytes
		 */
		static constexpr size_t chunkHeaderSize=8;
		/**
		 * The dummy module file header holds:
		 * - fixed Random IV : 8 bytes
		 */
		static constexpr size_t fileHeaderSize=8;

		/**
		 * The dummy module secret material is a key used to Xor blocks
		 * - size is 16 bytes
		 */
		static constexpr size_t secretMaterialSize=16;

		/**
		 * Store the file header and secret
		 */
		std::vector<uint8_t> m_fileHeader;
		std::vector<uint8_t> m_secret;
	public:
		/**
		 * @return the size in bytes of the chunk header
		 */
		size_t getChunkHeaderSize() const noexcept override {
			return chunkHeaderSize;
		}
		/**
		 * @return the size in bytes of file header module data
		 */
		size_t getModuleFileHeaderSize() const noexcept override {
			return fileHeaderSize;
		}
		/**
		 * @return the EncryptionSuite provided by this module
		 */
		EncryptionSuite getEncryptionSuite() const noexcept override {
		       return EncryptionSuite::dummy;
		}

		/**
		 * @return the secret material size
		 */
		size_t getSecretMaterialSize() const noexcept override {
			return secretMaterialSize;
		}

		/**
		 * Decrypt a chunk of data
		 * @param[in] a vector which size shall be chunkHeaderSize + chunkSize holding the raw data read from disk
		 * @return the decrypted data chunk
		 */
		std::vector<uint8_t> decryptChunk(const std::vector<uint8_t> &rawChunk) override ;

		void encryptChunk(std::vector<uint8_t> &rawChunk, const std::vector<uint8_t> &plainData) override;
		std::vector<uint8_t> encryptChunk(const uint32_t chunkIndex, const std::vector<uint8_t> &plainData) override;

		void setModuleFileHeader(std::vector<uint8_t> &fileHeader) override ;
		std::vector<uint8_t> getModuleFileHeader() const noexcept override ;

		void setModuleSecretMaterial(const std::vector<uint8_t> &secret) override ;

		VfsEncryptionModuleDummy();
		~VfsEncryptionModuleDummy() {};
};

} // namespace bctoolbox
#endif // BCTBX_VFS_ENCRYPTION_MODULE_DUMMY


