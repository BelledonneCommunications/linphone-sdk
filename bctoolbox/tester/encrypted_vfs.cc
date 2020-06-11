/*
 * Copyright (c) 2020 Belledonne Communications SARL.
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

#include "bctoolbox_tester.h"
#include "bctoolbox/vfs_encrypted.hh"
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
static std::string getHex(const uint8_t *buf, size_t size) {
	return getHex(std::vector<uint8_t>(buf, buf+size));
}

/* A callback to position the key material and algorithm suite to use */
static EncryptedVfsOpenCb set_dummy_encryption_info([](const std::string &filename, VfsEncryption &settings) {
	BCTBX_SLOGD<<"JOHAN: dummy encryption info callback in file is "<<filename;
	const std::vector<uint8_t> keyMaterial{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	settings.encryptionSuite_set(EncryptionSuite::dummy);
	settings.secretMaterial_set(keyMaterial);
});

void dummy_encryption_test() {
	/* set the encrypted vfs callback */
	VfsEncryption::openCallback_set(set_dummy_encryption_info);

	/* open an encrypted file */
	char *dummyFilePath = bc_tester_file("dummy.txt");
	bctbx_vfs_file_t *fp = bctbx_file_open2(&bcEncryptedVfs, dummyFilePath, O_RDWR|O_CREAT);

	uint8_t message[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
				0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f};
	uint8_t readBuffer[256];
	bctbx_file_write(fp, message, 4, 0);
	bctbx_file_write(fp, message, 16, 0);
	bctbx_file_write(fp, message, 16, 15);
	bctbx_file_read(fp, readBuffer, 16, 0);
	BCTBX_SLOGD<<"TEST JOHAN 1: read "<<getHex(readBuffer, 16);
	bctbx_file_read(fp, readBuffer, 16, 2);
	BCTBX_SLOGD<<"TEST JOHAN 2: read "<<getHex(readBuffer, 16);

	bctbx_file_write(fp, message, 32, 0);
	bctbx_file_write(fp, message, 33, 0);
	auto readSize = bctbx_file_read(fp, readBuffer, 160, 0);
	BCTBX_SLOGD<<"TEST JOHAN 3: read "<<readSize<<" bytes "<<getHex(readBuffer, readSize);
	bctbx_file_write(fp, message+32, 8, 49);
	readSize = bctbx_file_read(fp, readBuffer, 160, 0);
	BCTBX_SLOGD<<"TEST JOHAN 4: read "<<readSize<<" bytes "<<getHex(readBuffer, readSize);
	bctbx_file_truncate(fp, 120);
	readSize = bctbx_file_read(fp, readBuffer, 160, 0);
	BCTBX_SLOGD<<"TEST JOHAN 5: read "<<readSize<<" bytes "<<getHex(readBuffer, readSize);
	bctbx_file_truncate(fp, 120);
	readSize = bctbx_file_read(fp, readBuffer, 160, 0);
	BCTBX_SLOGD<<"TEST JOHAN 6: read "<<readSize<<" bytes "<<getHex(readBuffer, readSize);
	bctbx_file_truncate(fp, 32);
	readSize = bctbx_file_read(fp, readBuffer, 160, 0);
	BCTBX_SLOGD<<"TEST JOHAN 7: read "<<readSize<<" bytes "<<getHex(readBuffer, readSize);
	bctbx_file_truncate(fp, 27);
	readSize = bctbx_file_read(fp, readBuffer, 160, 0);
	BCTBX_SLOGD<<"TEST JOHAN 8: read "<<readSize<<" bytes "<<getHex(readBuffer, readSize);

	bctbx_free(dummyFilePath);
	bctbx_file_close(fp);

}

static test_t encrypted_vfs_tests[] = {
	TEST_NO_TAG("dummy_encryption", dummy_encryption_test)
};

test_suite_t encrypted_vfs_test_suite = {"Encrypted_vfs", NULL, NULL, NULL, NULL,
							   sizeof(encrypted_vfs_tests) / sizeof(encrypted_vfs_tests[0]), encrypted_vfs_tests};

