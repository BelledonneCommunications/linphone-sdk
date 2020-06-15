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
#include "bctoolbox/vfs_standard.h"
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

/**
 * Helper function : get encryptionSuite name
 */
static std::string suiteName(const bctoolbox::EncryptionSuite suite) {
	switch (suite) {
		case bctoolbox::EncryptionSuite::unset:
		      return "unset";
		case bctoolbox::EncryptionSuite::plain:
		      return "plain";
		case bctoolbox::EncryptionSuite::dummy:
		      return "dummy";
		default:
		      return "unknown";
	}
}

/* A callback to position the key material and algorithm suite to use */
static EncryptedVfsOpenCb set_dummy_encryption_info([](VfsEncryption &settings) {
	BCTBX_SLOGD<<"JOHAN: dummy encryption info callback in file is "<<settings.filename_get();
	const std::vector<uint8_t> keyMaterial{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	settings.encryptionSuite_set(EncryptionSuite::dummy);
	settings.secretMaterial_set(keyMaterial);
	settings.chunkSize_set(16);
});

static EncryptedVfsOpenCb set_plain_encryption_info([](VfsEncryption &settings) {
	BCTBX_SLOGD<<"JOHAN: plain encryption info callback in file is "<<settings.filename_get();
	settings.encryptionSuite_set(EncryptionSuite::plain);
});


static EncryptedVfsOpenCb set_encryption_info([](VfsEncryption &settings) {
	auto filename = settings.filename_get();
	if (filename.find("plain") != std::string::npos) {
		set_plain_encryption_info(settings);
	} else { // default to dummy
		set_dummy_encryption_info(settings);
	}
});
/* a message to write in files */
static const uint8_t message[256] = {0x42, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
				0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
				0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
				0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
				0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
				0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
				0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
				0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
				0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
				0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
				0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
				0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
				0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
				0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
				0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
				0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

/**
 * Block size for tests is 16 bytes
 * if closeFile is true, close and reopen file afet each write operation
 */
void basic_encryption_test(bctoolbox::EncryptionSuite suite, bool closeFile = true) {
	// used to check zero readings
	uint8_t zero_buff[256];
	memset (zero_buff, 0, sizeof(zero_buff));

	/* get the encrypted file path */
	char *path = bc_tester_file("basic.");
	std::string filePath{path};
	filePath.append(suiteName(suite)).append(".evfs");
	bctbx_free(path);

	/* remove file if it was already there */
	remove(filePath.data());

	/* create the file */
	bctbx_vfs_file_t *fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);

	uint8_t readBuffer[256];
	memset(readBuffer, 0, sizeof(readBuffer));

	/* Make simple write, size is < block size and check we get back what we just wrote */
	bctbx_file_write(fp, message, 4, 0);
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}

	BC_ASSERT_EQUAL(bctbx_file_size(fp), 4, size_t, "%ld");
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 4, 0), 4, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, message, 4)==0);
	memset(readBuffer, 0, sizeof(readBuffer));
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 32, 0), 4, ssize_t, "%ld"); // try to read more than we have
	BC_ASSERT_TRUE(memcmp(readBuffer, message, 4)==0);
	memset(readBuffer, 0, sizeof(readBuffer));
	

	/* Write a whole block completely overwritting previous data */
	bctbx_file_write(fp, message, 16, 0);
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}

	BC_ASSERT_EQUAL(bctbx_file_size(fp), 16, size_t, "%ld");
	bctbx_file_read(fp, readBuffer, 16, 0);
	BC_ASSERT_TRUE(memcmp(readBuffer, message, 4)==0);
	memset(readBuffer, 0, sizeof(readBuffer));

	/* Partial overwrite of the end of first block */
	bctbx_file_write(fp, message, 8, 8);
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}

	BC_ASSERT_EQUAL(bctbx_file_size(fp), 16, size_t, "%ld");
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 0), 16, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, message, 8)==0);
	BC_ASSERT_TRUE(memcmp(readBuffer+8, message, 8)==0);
	memset(readBuffer, 0, sizeof(readBuffer));
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 8), 8, ssize_t, "%ld"); // read at index not 8, we have only 8 bytes to get ask for 16 anyway
	BC_ASSERT_TRUE(memcmp(readBuffer, message, 8)==0);
	memset(readBuffer, 0, sizeof(readBuffer));

	/* Partial overwrite of the begining of first block */
	bctbx_file_write(fp, message+16, 8, 0);
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}

	BC_ASSERT_EQUAL(bctbx_file_size(fp), 16, size_t, "%ld");
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 0), 16, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, message+16, 8)==0);
	BC_ASSERT_TRUE(memcmp(readBuffer+8, message, 8)==0);
	memset(readBuffer, 0, sizeof(readBuffer));

	/* truncate the file to a size smaller than it was */
	BC_ASSERT_EQUAL(bctbx_file_truncate(fp,8), 0, size_t, "%ld");
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}

	BC_ASSERT_EQUAL(bctbx_file_size(fp), 8, size_t, "%ld");
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 8), 0, ssize_t, "%ld"); // read after the end of the file -> nothing shall get back
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 0), 8, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, message+16, 8)==0);
	memset(readBuffer, 0, sizeof(readBuffer));

	/* truncate the file to a size 0 */
	BC_ASSERT_EQUAL(bctbx_file_truncate(fp,0), 0, size_t, "%ld");
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}

	BC_ASSERT_EQUAL(bctbx_file_size(fp), 0, size_t, "%ld");
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 0), 0, ssize_t, "%ld");


	/* write on several blocks, starting after the current end of file */
	bctbx_file_write(fp, message, 65, 15);
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}

	BC_ASSERT_EQUAL(bctbx_file_size(fp), 80, size_t, "%ld");
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 80, 0), 80, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, zero_buff, 15)==0);
	BC_ASSERT_TRUE(memcmp(readBuffer+15, message, 65)==0);
	memset(readBuffer, 0, sizeof(readBuffer));

	/* overwrite on several blocks */
	bctbx_file_write(fp, message, 18, 31);
	if (closeFile) {
		bctbx_file_close(fp);
		fp = bctbx_file_open2(&bcEncryptedVfs, filePath.data(), O_RDWR|O_CREAT);
	}
	BC_ASSERT_EQUAL(bctbx_file_size(fp), 80, size_t, "%ld");
	// read the part left there from offset 15 to 31
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 15), 16, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, message, 16)==0);
	memset(readBuffer, 0, sizeof(readBuffer));
	// read the part we just wrote
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 18, 31), 18, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, message, 18)==0);
	memset(readBuffer, 0, sizeof(readBuffer));
	// read the part left after
	BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 80, 49), 31, ssize_t, "%ld");
	BC_ASSERT_TRUE(memcmp(readBuffer, message+34, 31)==0);
	memset(readBuffer, 0, sizeof(readBuffer));

	bctbx_file_close(fp);

	// When using the plain, check with the standard the content of file
	if (suite == bctoolbox::EncryptionSuite::plain ) {
		fp = bctbx_file_open2(&bcStandardVfs, filePath.data(), O_RDWR);

		BC_ASSERT_EQUAL(bctbx_file_size(fp), 80, size_t, "%ld");
		// read the part left there from offset 15 to 31
		BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 16, 15), 16, ssize_t, "%ld");
		BC_ASSERT_TRUE(memcmp(readBuffer, message, 16)==0);
		memset(readBuffer, 0, sizeof(readBuffer));
		// read the part we just wrote
		BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 18, 31), 18, ssize_t, "%ld");
		BC_ASSERT_TRUE(memcmp(readBuffer, message, 18)==0);
		memset(readBuffer, 0, sizeof(readBuffer));
		// read the part left after
		BC_ASSERT_EQUAL(bctbx_file_read(fp, readBuffer, 80, 49), 31, ssize_t, "%ld");
		BC_ASSERT_TRUE(memcmp(readBuffer, message+34, 31)==0);
		memset(readBuffer, 0, sizeof(readBuffer));

		bctbx_file_close(fp);
	}
}

void basic_encryption_test() {
	/* set the encrypted vfs callback */
	VfsEncryption::openCallback_set(set_encryption_info);

	basic_encryption_test(EncryptionSuite::dummy, false);
	basic_encryption_test(EncryptionSuite::dummy, true);
	basic_encryption_test(EncryptionSuite::plain, false);
	basic_encryption_test(EncryptionSuite::plain, true);
}

static test_t encrypted_vfs_tests[] = {
	TEST_NO_TAG("basic", basic_encryption_test)
};

test_suite_t encrypted_vfs_test_suite = {"Encrypted vfs", NULL, NULL, NULL, NULL,
							   sizeof(encrypted_vfs_tests) / sizeof(encrypted_vfs_tests[0]), encrypted_vfs_tests};

