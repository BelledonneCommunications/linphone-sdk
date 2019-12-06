/*
 * Copyright (c) 2014-2019 Belledonne Communications SARL.
 *
 * This file is part of bzrtp.
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "bzrtp/bzrtp.h"
#include "cryptoUtils.h"
#include "testUtils.h"
#include "bzrtpTest.h"

/**
 *
 * Patterns generated using GNU ZRTP C++
*/
#define KDF_TEST_NUMBER 9
uint8_t patternKDFLabel[KDF_TEST_NUMBER][28] = {
	"Responder SRTP master key", "Responder SRTP master salt", "SAS", "ZRTP Session Key", "retained secret", "Responder ZRTP key", "Initiator ZRTP key", "Responder HMAC key", "Initiator HMAC key"};

uint16_t patternKDFContextLength[KDF_TEST_NUMBER] = {56, 56, 56 , 56, 56, 56, 56, 56, 56};

uint16_t patternKDFHmacLength[KDF_TEST_NUMBER] = {16, 14, 32, 32, 32, 16, 16, 32, 32};

uint8_t patternKDFOutput[KDF_TEST_NUMBER][32] = {
	{0xf2, 0xcf, 0xc5, 0x72, 0x4a, 0xcf, 0xd7, 0xb5, 0x0e, 0x89, 0xbe, 0xd8, 0xa5, 0xc9, 0xf2, 0xcd},
	{0x3c, 0x8f, 0xb1, 0x3f, 0xd6, 0xca, 0x83, 0xfb, 0xd6, 0xaf, 0x3b, 0xd5, 0xab, 0x1c},
	{0xd2, 0x69, 0xb0, 0x71, 0x8a, 0xd5, 0x8f, 0x3c, 0x8e, 0x0d, 0xcf, 0x45, 0x35, 0x81, 0x1d, 0x95, 0x18, 0x47, 0x31, 0x41, 0x28, 0x31, 0x9f, 0x54, 0xc6, 0x06, 0x59, 0x5b, 0x52, 0x2c, 0x52, 0x87},
	{0xc3, 0x63, 0x39, 0x0b, 0xc9, 0xee, 0x13, 0xaa, 0x9e, 0x6a, 0x00, 0x52, 0x84, 0x09, 0xdd, 0xc9, 0x07, 0x17, 0xe0, 0x1d, 0x1a, 0xe2, 0xdc, 0xf9, 0xa7, 0x0a, 0xb9, 0x88, 0x86, 0xac, 0x69, 0x75},
	{0xa7, 0x27, 0x2d, 0xee, 0x77, 0xe7, 0x99, 0x0c, 0x24, 0xb2, 0x0b, 0x9f, 0x54, 0xba, 0x2b, 0x61, 0x7a, 0x23, 0xd8, 0xa5, 0x97, 0x65, 0x82, 0x9b, 0x78, 0x07, 0x2b, 0xc5, 0x12, 0x99, 0x01, 0x5c},
	{0x75, 0x76, 0x7d, 0x41, 0x82, 0x9c, 0xb6, 0x97, 0xc7, 0xa2, 0x0d, 0x68, 0x12, 0xea, 0xdb, 0x9a},
	{0x08, 0xa8, 0x21, 0x46, 0x0c, 0xbd, 0x6c, 0xe0, 0xf1, 0xfe, 0x3c, 0x4c, 0x70, 0x4e, 0x3b, 0xea},
	{0x21, 0x69, 0xf1, 0x5c, 0x1b, 0x85, 0xa2, 0x39, 0x77, 0x02, 0x4d, 0x5e, 0xa7, 0x1f, 0x48, 0x16, 0x23, 0xfe, 0x94, 0xdc, 0x1a, 0xab, 0xf4, 0x89, 0xb8, 0x74, 0x4d, 0x0d, 0x37, 0x3d, 0x7c, 0xab},
	{0x1d, 0x11, 0x7b, 0xef, 0xd6, 0x27, 0xcc, 0x5a, 0x80, 0xd0, 0x23, 0x6e, 0x8d, 0x49, 0x6b, 0xe4, 0x2a, 0x6a, 0x01, 0x18, 0xac, 0x41, 0x59, 0xe8, 0x61, 0xb9, 0xb1, 0x4a, 0x78, 0x4b, 0x5d, 0x3f}
};

uint8_t patternKDFContext[KDF_TEST_NUMBER][56] = {
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f},
	{0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x7d, 0xc4, 0x35, 0xa7, 0x4d, 0x75, 0x25, 0x44, 0x80, 0xc2, 0x11, 0xf2, 0xd3, 0x20, 0x5e, 0xa7, 0x1b, 0x4b, 0xa4, 0xee, 0xc7, 0x8e, 0xb7, 0x35, 0x75, 0x28, 0xe5, 0x6f, 0xcf, 0x4f, 0x74, 0x9f}
};

static void test_zrtpKDF(void) {
	int i;
	uint8_t keyKDF[32] = {0x33, 0xe6, 0x6c, 0x01, 0xca, 0x6f, 0xe6, 0x4f, 0xb7, 0x6f, 0xfd, 0xe3, 0x1c, 0xab, 0xc0, 0xfb, 0xad, 0x3d, 0x31, 0x02, 0x67, 0x6b, 0x0c, 0x09, 0x0f, 0xc9, 0x96, 0x38, 0x1e, 0x0a, 0x8c, 0x2f};
	uint8_t output[32];

	for (i=0; i<KDF_TEST_NUMBER; i++) {
		bzrtp_keyDerivationFunction(keyKDF, 32,
			patternKDFLabel[i], strlen((char *)patternKDFLabel[i]),
			patternKDFContext[i], patternKDFContextLength[i],
			patternKDFHmacLength[i],
			bctbx_hmacSha256,
			output);

		BC_ASSERT_TRUE(memcmp(output, patternKDFOutput[i], patternKDFHmacLength[i]) == 0);
	}
}

#define CRC_TEST_NUMBER 3
uint16_t patternCRCLength[CRC_TEST_NUMBER] = {128, 24, 480};

uint8_t patterCRCinput[CRC_TEST_NUMBER][480] = {
 {0x10, 0x00, 0x18, 0xca, 0x5a, 0x52, 0x54, 0x50, 0x63, 0x67, 0x4b, 0xfb, 0x50, 0x5a, 0x00, 0x1d, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x20, 0x20, 0x31, 0x2e, 0x31, 0x30, 0x4c, 0x49, 0x4e, 0x50, 0x48, 0x4f, 0x4e, 0x45, 0x2d, 0x5a, 0x52, 0x54, 0x50, 0x43, 0x50, 0x50, 0x5d, 0x9c, 0x1a, 0x31, 0xfa, 0x1d, 0xe7, 0xfd, 0x0a, 0x90, 0x33, 0x92, 0x3a, 0x7b, 0xe5, 0xfb, 0xf9, 0xa3, 0xd3, 0x23, 0x28, 0x6e, 0xfc, 0x72, 0x49, 0xeb, 0x7b, 0x71, 0x3e, 0x06, 0x7c, 0xc9, 0x94, 0xa9, 0x06, 0x90, 0xcf, 0x9c, 0xed, 0x13, 0x99, 0x8c, 0x51, 0x34, 0x00, 0x01, 0x12, 0x21, 0x53, 0x32, 0x35, 0x36, 0x41, 0x45, 0x53, 0x31, 0x48, 0x53, 0x33, 0x32, 0x48, 0x53, 0x38, 0x30, 0x44, 0x48, 0x33, 0x6b, 0x4d, 0x75, 0x6c, 0x74, 0x42, 0x33, 0x32, 0x20, 0x80, 0x47, 0x37, 0x13, 0xdc, 0xd6, 0xef, 0x54},
 {0x10, 0x00, 0x18, 0xcd, 0x5a, 0x52, 0x54, 0x50, 0x63, 0x67, 0x4b, 0xfb, 0x50, 0x5a, 0x00, 0x03, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x41, 0x43, 0x4b},
 {0x10, 0x00, 0x18, 0xcf, 0x5a, 0x52, 0x54, 0x50, 0x63, 0x67, 0x4b, 0xfb, 0x50, 0x5a, 0x00, 0x75, 0x44, 0x48, 0x50, 0x61, 0x72, 0x74, 0x31, 0x20, 0xda, 0x39, 0xde, 0xa2, 0x99, 0xf2, 0x9e, 0x17, 0x7b, 0xd2, 0x5e, 0x16, 0x21, 0xfb, 0xf3, 0xe7, 0x8f, 0xa7, 0xdf, 0x0d, 0x18, 0x79, 0x86, 0x45, 0x2d, 0x9c, 0xf2, 0x16, 0x7f, 0x09, 0x98, 0x11, 0x3a, 0xd1, 0xeb, 0xf1, 0x85, 0xa7, 0x2a, 0x85, 0xfe, 0x41, 0x5f, 0x50, 0x80, 0x5a, 0xaa, 0x39, 0x27, 0x89, 0xc0, 0x58, 0x68, 0x1c, 0x56, 0xae, 0x36, 0xa6, 0xee, 0x54, 0x06, 0x2c, 0x06, 0x76, 0x7c, 0x8d, 0xe0, 0xf7, 0x66, 0x82, 0x85, 0xb6, 0x3a, 0x2e, 0xd1, 0x98, 0x9d, 0xd9, 0xf1, 0x89, 0x67, 0xac, 0xe2, 0xe6, 0x54, 0x50, 0x7e, 0x24, 0xdd, 0x9b, 0xa3, 0x0d, 0xbf, 0xe8, 0x65, 0xd3, 0xe3, 0x4b, 0x76, 0x2b, 0xec, 0x28, 0x42, 0xf7, 0x09, 0x35, 0x79, 0x42, 0xb5, 0x25, 0x57, 0x2b, 0x53, 0x71, 0xe1, 0xca, 0x40, 0x25, 0xa9, 0x98, 0x21, 0x3b, 0xfb, 0xbd, 0x14, 0xf7, 0xb9, 0xbb, 0x5c, 0x51, 0xca, 0xdc, 0xe2, 0xe6, 0xf7, 0x19, 0x69, 0xe4, 0x48, 0x93, 0x73, 0x06, 0x17, 0x0a, 0x04, 0x6a, 0x50, 0xa1, 0x51, 0xd5, 0x73, 0xf6, 0xae, 0xbb, 0xaa, 0xb5, 0xd1, 0x40, 0x5a, 0x8e, 0x28, 0x15, 0x02, 0xff, 0xd6, 0xa2, 0x0f, 0x64, 0x8e, 0xed, 0x74, 0x7f, 0x4d, 0xdc, 0x93, 0x0e, 0x87, 0xfe, 0x9e, 0x31, 0x86, 0xe3, 0x30, 0x88, 0x67, 0x1f, 0x68, 0x61, 0xb7, 0x26, 0x34, 0x14, 0xb1, 0xf5, 0x97, 0x99, 0x53, 0xeb, 0xf1, 0x55, 0x1b, 0xf4, 0xa6, 0xe9, 0x21, 0xdc, 0x2b, 0xc4, 0x1c, 0x26, 0x7e, 0x86, 0x11, 0x80, 0xe3, 0xa2, 0xd8, 0x2a, 0x64, 0x4f, 0x1b, 0x45, 0xf8, 0x90, 0x5a, 0x30, 0x2a, 0xc8, 0x13, 0xb9, 0xd7, 0x2b, 0x32, 0x56, 0xc9, 0xea, 0x9f, 0xef, 0xbb, 0xf7, 0xc3, 0x43, 0x9e, 0x27, 0x12, 0xbd, 0xec, 0x1e, 0x61, 0x1d, 0xc6, 0x58, 0xf3, 0x77, 0x2a, 0x27, 0x9a, 0xb7, 0xf1, 0x1e, 0x15, 0x2b, 0x0d, 0x67, 0x06, 0x07, 0x65, 0x6d, 0x89, 0x60, 0x29, 0xa1, 0x49, 0x25, 0xd4, 0x50, 0x6e, 0xfe, 0xe2, 0xa8, 0x8e, 0x59, 0xf9, 0xb3, 0x5f, 0x98, 0x0d, 0x29, 0x87, 0xd3, 0x87, 0x45, 0x35, 0xa4, 0x3f, 0x87, 0x5f, 0x6f, 0xd0, 0x4a, 0xd2, 0xf9, 0x37, 0x68, 0x7e, 0xc6, 0xaa, 0x57, 0x3e, 0x71, 0xaf, 0xb4, 0x48, 0xea, 0x0e, 0x7e, 0x46, 0xf9, 0xdb, 0xd3, 0xb9, 0x26, 0xc4, 0xe2, 0x50, 0x34, 0x87, 0x0a, 0xb1, 0x0d, 0x92, 0x01, 0x0a, 0xb1, 0xb0, 0xd5, 0xd9, 0xb7, 0x74, 0x01, 0x2e, 0x0f, 0xab, 0xc8, 0x4a, 0x57, 0x0e, 0x94, 0x2c, 0x2d, 0x32, 0x7f, 0x67, 0x89, 0xa2, 0xc5, 0xa2, 0xb3, 0xd3, 0x68, 0x76, 0x3d, 0xe8, 0x96, 0x34, 0xa9, 0x96, 0x4f, 0x66, 0xc0, 0x10, 0x3c, 0xf6, 0x4d, 0xaf, 0xc8, 0xd3, 0xbd, 0x38, 0x6f, 0x52, 0x0f, 0x1b, 0xa9, 0x0e, 0x92, 0x46, 0x0d, 0xba, 0x40, 0x1a, 0xec, 0x68, 0x80, 0xec, 0xa0, 0xfe, 0x1a, 0x9b, 0xa5, 0xd8, 0x54, 0x85, 0x15, 0x9e, 0x64, 0x1c, 0x5e, 0xd8, 0x97, 0xc2, 0x4c, 0x4f, 0xf7, 0xe9, 0x09, 0x3c, 0x6d, 0xc3, 0x8c, 0x1b, 0x36, 0xd0, 0x56, 0xd2, 0x77, 0xcd, 0xaa, 0xcf, 0x5c, 0xff, 0xdf, 0x23, 0x93, 0xb5, 0xdc, 0x08, 0x95, 0x97, 0xdb, 0x68, 0x2d, 0x4b, 0x01, 0x89, 0xb2, 0x8b, 0xd0, 0x25, 0x19, 0xab, 0xbf, 0x49, 0xcf, 0xa5, 0x27}
};

uint32_t patternCRCoutput[CRC_TEST_NUMBER] = {0x5065ab04, 0x9c9edccd, 0xda5c92ea};


/**
 *
 * Pattern generated using GNU-ZRTP C++
 *
 */
static void test_CRC32(void) {
	int i;
	for (i=0; i<CRC_TEST_NUMBER; i++) {
		BC_ASSERT_TRUE(bzrtp_CRC32(patterCRCinput[i], patternCRCLength[i]) == patternCRCoutput[i]);
	}
}
/* expected result of tests vary according to default key agreement algorithm */
/* if EC25519 is available, this is the default, DH3k otherwise */
static uint8_t getDefaultKeyAgreementAlgo() {
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		return ZRTP_KEYAGREEMENT_X255;
	}
	return ZRTP_KEYAGREEMENT_DH3k;
}
static void setHelloMessageAlgo(bzrtpHelloMessage_t *helloMessage, uint8_t algoType, uint8_t types[7], const uint8_t typesCount) {
	switch(algoType) {
		case ZRTP_HASH_TYPE:
			memcpy(helloMessage->supportedHash, types, typesCount);
			helloMessage->hc = typesCount;
			break;
		case ZRTP_CIPHERBLOCK_TYPE:
			memcpy(helloMessage->supportedCipher, types, typesCount);
			helloMessage->cc = typesCount;
			break;
		case ZRTP_AUTHTAG_TYPE:
			memcpy(helloMessage->supportedAuthTag, types, typesCount);
			helloMessage->ac = typesCount;
			break;
		case ZRTP_KEYAGREEMENT_TYPE:
			memcpy(helloMessage->supportedKeyAgreement, types, typesCount);
			helloMessage->kc = typesCount;
			break;
		case ZRTP_SAS_TYPE:
			memcpy(helloMessage->supportedSas, types, typesCount);
			helloMessage->sc = typesCount;
			break;
	}
}

static int compareAllAlgoTypes(bzrtpChannelContext_t *zrtpChannelContext, uint8_t expectedHashAlgo, uint8_t expectedCipherAlgo, uint8_t expectedAuthTagAlgo, uint8_t expectedKeyAgreementAlgo, uint8_t expectedSasAlgo) {
	return
			zrtpChannelContext->hashAlgo         == expectedHashAlgo &&
			zrtpChannelContext->cipherAlgo       == expectedCipherAlgo &&
			zrtpChannelContext->authTagAlgo      == expectedAuthTagAlgo &&
			zrtpChannelContext->keyAgreementAlgo == expectedKeyAgreementAlgo &&
			zrtpChannelContext->sasAlgo          == expectedSasAlgo;
}

static int compareAllAlgoTypesWithExpectedChangedOnly(bzrtpChannelContext_t *zrtpChannelContext, uint8_t expectedAlgoType, uint8_t expectedType) {
	switch(expectedAlgoType) {
		case ZRTP_HASH_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, expectedType,   ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, getDefaultKeyAgreementAlgo(), ZRTP_SAS_B32);
		case ZRTP_CIPHERBLOCK_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, expectedType,     ZRTP_AUTHTAG_HS32, getDefaultKeyAgreementAlgo(), ZRTP_SAS_B32);
		case ZRTP_AUTHTAG_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, expectedType,      getDefaultKeyAgreementAlgo(), ZRTP_SAS_B32);
		case ZRTP_KEYAGREEMENT_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, expectedType,           ZRTP_SAS_B32);
		case ZRTP_SAS_TYPE:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, getDefaultKeyAgreementAlgo(), expectedType);
		default:
			return compareAllAlgoTypes(zrtpChannelContext, ZRTP_HASH_S256, ZRTP_CIPHER_AES1, ZRTP_AUTHTAG_HS32, getDefaultKeyAgreementAlgo(), ZRTP_SAS_B32);
	}
}

static int testAlgoType(uint8_t algoType, uint8_t *packetTypes, uint8_t packetTypesCount, uint8_t *contextTypes, uint8_t contextTypesCount, uint8_t expectedType) {
	int retval;

	bzrtpContext_t *zrtpContext = bzrtp_createBzrtpContext();
	bzrtpPacket_t *helloPacket = NULL;
	bzrtp_initBzrtpContext(zrtpContext, 0x12345678);

	if (contextTypes != NULL) {
		bzrtp_setSupportedCryptoTypes(zrtpContext, algoType, contextTypes, contextTypesCount);
	}

	helloPacket = bzrtp_createZrtpPacket(zrtpContext, zrtpContext->channelContext[0], MSGTYPE_HELLO, &retval);
	if (packetTypes != NULL) {
		bzrtpHelloMessage_t *helloMessage = (bzrtpHelloMessage_t *)helloPacket->messageData;
		setHelloMessageAlgo(helloMessage, algoType, packetTypes, packetTypesCount);
	}

	BC_ASSERT_FALSE(bzrtp_cryptoAlgoAgreement(zrtpContext, zrtpContext->channelContext[0], helloPacket->messageData));
	retval = compareAllAlgoTypesWithExpectedChangedOnly(zrtpContext->channelContext[0], algoType, expectedType);

	bzrtp_freeZrtpPacket(helloPacket);
	bzrtp_destroyBzrtpContext(zrtpContext, 0x12345678);
	return retval;
}

static int testAlgoTypeWithPacket(uint8_t algoType, uint8_t *types, uint8_t typesCount, uint8_t expectedType) {
	return testAlgoType(algoType, types, typesCount, NULL, 0, expectedType);
}

static int testAlgoTypeWithContext(uint8_t algoType, uint8_t *types, uint8_t typesCount, uint8_t expectedType) {
	return testAlgoType(algoType, NULL, 0, types, typesCount, expectedType);
}

struct st_algo_type_with_packet {
	uint8_t types[7];
	uint8_t typesCount;
	uint8_t expectedType;
};

struct st_algo_type_with_context {
	uint8_t types[7];
	uint8_t typesCount;
	uint8_t expectedType;
};

struct st_algo_type {
	uint8_t packetTypes[7];
	uint8_t packetTypesCount;
	uint8_t contextTypes[7];
	uint8_t contextTypesCount;
	uint8_t expectedType;
};

static void test_algoAgreement(void) {
	struct st_algo_type_with_packet agreement_types_with_packet[] = {
		{ {ZRTP_KEYAGREEMENT_DH2k}, 1, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH3k}, 1, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_EC52}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_DH3k}, 3, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_packet *agreement_type_with_packet;
	struct st_algo_type_with_context agreement_types_with_context[] = {
		{ {ZRTP_KEYAGREEMENT_DH2k}, 1, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH3k}, 1, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC25}, 1, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_EC52}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_DH3k}, 3, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_context *agreement_type_with_context;
	struct st_algo_type agreement_types[] = {
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, ZRTP_KEYAGREEMENT_DH3k },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 4, ZRTP_KEYAGREEMENT_DH2k },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type *agreement_type;
	struct st_algo_type_with_packet cipher_types_with_packet[] = {
		{ {ZRTP_CIPHER_AES1}, 1, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3}, 1, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 3, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES3}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_packet *cipher_type_with_packet;
	struct st_algo_type_with_context cipher_types_with_context[] = {
		{ {ZRTP_CIPHER_AES1}, 1, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3}, 1, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS1}, 1, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 3, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1}, 3, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES3}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type_with_context *cipher_type_with_context;
	struct st_algo_type cipher_types[] = {
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, ZRTP_CIPHER_AES1 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, ZRTP_CIPHER_AES3 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES3}, 6, ZRTP_CIPHER_AES1 },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, 0 }
	};
	struct st_algo_type *cipher_type;

	/* check defaults */
	BC_ASSERT_TRUE(testAlgoTypeWithPacket(ZRTP_UNSET_ALGO, NULL, 0, getDefaultKeyAgreementAlgo()));

	/* key agreement type */
	agreement_type_with_packet = &agreement_types_with_packet[0];
	while (agreement_type_with_packet->typesCount > 0) {
		BC_ASSERT_TRUE(testAlgoTypeWithPacket(ZRTP_KEYAGREEMENT_TYPE, agreement_type_with_packet->types, agreement_type_with_packet->typesCount, agreement_type_with_packet->expectedType));
		agreement_type_with_packet++;
	}
	agreement_type_with_context = &agreement_types_with_context[0];
	while (agreement_type_with_context->typesCount > 0) {
		BC_ASSERT_TRUE(testAlgoTypeWithContext(ZRTP_KEYAGREEMENT_TYPE, agreement_type_with_context->types, agreement_type_with_context->typesCount, agreement_type_with_context->expectedType));
		agreement_type_with_context++;
	}
	agreement_type = &agreement_types[0];
	while (agreement_type->packetTypesCount > 0) {
		BC_ASSERT_TRUE(testAlgoType(ZRTP_KEYAGREEMENT_TYPE, agreement_type->packetTypes, agreement_type->packetTypesCount, agreement_type->contextTypes, agreement_type->contextTypesCount, agreement_type->expectedType));
		agreement_type++;
	}

	/* cipher type */
	cipher_type_with_packet = &cipher_types_with_packet[0];
	while (cipher_type_with_packet->typesCount > 0) {
		BC_ASSERT_TRUE(testAlgoTypeWithPacket(ZRTP_CIPHERBLOCK_TYPE, cipher_type_with_packet->types, cipher_type_with_packet->typesCount, cipher_type_with_packet->expectedType));
		cipher_type_with_packet++;
	}
	cipher_type_with_context = &cipher_types_with_context[0];
	while (cipher_type_with_context->typesCount > 0) {
		BC_ASSERT_TRUE(testAlgoTypeWithContext(ZRTP_CIPHERBLOCK_TYPE, cipher_type_with_context->types, cipher_type_with_context->typesCount, cipher_type_with_context->expectedType));
		cipher_type_with_context++;
	}
	cipher_type = &cipher_types[0];
	while (cipher_type->packetTypesCount > 0) {
		BC_ASSERT_TRUE(testAlgoType(ZRTP_CIPHERBLOCK_TYPE, cipher_type->packetTypes, cipher_type->packetTypesCount, cipher_type->contextTypes, cipher_type->contextTypesCount, cipher_type->expectedType));
		cipher_type++;
	}
}

static int compareAlgoTypes(uint8_t actualTypes[7], uint8_t actualTypesCount, uint8_t expectedTypes[7], uint8_t expectedTypesCount) {
	int i;

	if (actualTypesCount != expectedTypesCount) {
		return 0;
	}

	for (i=0; i<actualTypesCount; i++) {
		if (actualTypes[i] != expectedTypes[i]) {
			return 0;
		}
	}

	return 1;
}

static int testAlgoSetterGetter(uint8_t algoType, uint8_t *contextTypes, uint8_t contextTypesCount, uint8_t *expectedTypes, uint8_t expectedTypesCount) {
	int retval;
	uint8_t compareTypes[8];
	uint8_t compareTypesCount;

	bzrtpContext_t *zrtpContext = bzrtp_createBzrtpContext();
	bzrtp_initBzrtpContext(zrtpContext, 0x12345678);
	bzrtp_setSupportedCryptoTypes(zrtpContext, algoType, contextTypes, contextTypesCount);
	compareTypesCount = bzrtp_getSupportedCryptoTypes(zrtpContext, algoType, compareTypes);
	retval = compareAlgoTypes(compareTypes, compareTypesCount, expectedTypes, expectedTypesCount);
	bzrtp_destroyBzrtpContext(zrtpContext, 0x12345678);
	return retval;
}

struct st_algo_setter_getter {
	uint8_t contextTypes[8];
	uint8_t contextTypesCount;
	uint8_t expectedTypes[8];
	uint8_t expectedTypesCount;
};

static void test_algoSetterGetter(void) {
	struct st_algo_setter_getter agreement_types[] = {
		{ {ZRTP_KEYAGREEMENT_DH2k}, 1, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 3 },
		{ {ZRTP_KEYAGREEMENT_DH3k}, 1, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_EC25}, 1, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_EC38}, 1, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_EC52}, 1, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_Prsh}, 1, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_Mult}, 1, {ZRTP_KEYAGREEMENT_Mult, ZRTP_KEYAGREEMENT_DH3k}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_Mult}, 3 },
		{ {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 3 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_EC25}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_EC52}, 2, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_Mult}, 3 },
		{ {ZRTP_KEYAGREEMENT_Prsh, ZRTP_KEYAGREEMENT_EC52, ZRTP_KEYAGREEMENT_EC38, ZRTP_KEYAGREEMENT_EC25, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 6, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_Mult}, 3},
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k}, 4, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH2k, ZRTP_KEYAGREEMENT_Mult}, 3 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_DH3k}, 8, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 2 },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, }
	};
	/* Define another set used only if ECDH agreements are availables */
	struct st_algo_setter_getter ecdh_agreement_types[] = {
		{ {ZRTP_KEYAGREEMENT_X255}, 1, {ZRTP_KEYAGREEMENT_X255, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 3 },
		{ {ZRTP_KEYAGREEMENT_X448}, 1, {ZRTP_KEYAGREEMENT_X448, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 3 },
		{ {ZRTP_KEYAGREEMENT_X255, ZRTP_KEYAGREEMENT_X448}, 2, {ZRTP_KEYAGREEMENT_X255, ZRTP_KEYAGREEMENT_X448, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 4 },
		{ {ZRTP_KEYAGREEMENT_X448, ZRTP_KEYAGREEMENT_X255}, 2, {ZRTP_KEYAGREEMENT_X448, ZRTP_KEYAGREEMENT_X255, ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_Mult}, 4 },
		{ {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_X448, ZRTP_KEYAGREEMENT_X255}, 3, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_X448, ZRTP_KEYAGREEMENT_X255, ZRTP_KEYAGREEMENT_Mult}, 4 },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, }
	};
	struct st_algo_setter_getter *agreement_type;
	struct st_algo_setter_getter cipher_types[] = {
		{ {ZRTP_CIPHER_AES1}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES2}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES3}, 1, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_CIPHER_2FS1}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_2FS2}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_2FS3}, 1, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES3}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2, {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES3}, 2 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_AES2}, 2, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES1, ZRTP_CIPHER_2FS3}, 2, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_AES2, ZRTP_CIPHER_2FS3}, 2, {ZRTP_CIPHER_AES1}, 1 },
		{ {ZRTP_CIPHER_2FS3, ZRTP_CIPHER_2FS2, ZRTP_CIPHER_2FS1, ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES2, ZRTP_CIPHER_AES1}, 6, {ZRTP_CIPHER_AES3, ZRTP_CIPHER_AES1}, 2 },
		{ {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, }
	};
	struct st_algo_setter_getter *cipher_type;

	/* key agreement type */
	agreement_type = &agreement_types[0];
	while (agreement_type->contextTypesCount > 0) {
		BC_ASSERT_TRUE(testAlgoSetterGetter(ZRTP_KEYAGREEMENT_TYPE, agreement_type->contextTypes, agreement_type->contextTypesCount, agreement_type->expectedTypes, agreement_type->expectedTypesCount));
		agreement_type++;
	}

	/* ECDH agreement type */
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		agreement_type = &ecdh_agreement_types[0];
		while (agreement_type->contextTypesCount > 0) {
			BC_ASSERT_TRUE(testAlgoSetterGetter(ZRTP_KEYAGREEMENT_TYPE, agreement_type->contextTypes, agreement_type->contextTypesCount, agreement_type->expectedTypes, agreement_type->expectedTypesCount));
			agreement_type++;
		}
	}

	/* cipher type */
	cipher_type = &cipher_types[0];
	while (cipher_type->contextTypesCount > 0) {
		BC_ASSERT_TRUE(testAlgoSetterGetter(ZRTP_CIPHERBLOCK_TYPE, cipher_type->contextTypes, cipher_type->contextTypesCount, cipher_type->expectedTypes, cipher_type->expectedTypesCount));
		cipher_type++;
	}
}

#define ZRTP_AUTHTAG_FAKE_1		0x41
#define ZRTP_AUTHTAG_FAKE_2		0x42
#define ZRTP_AUTHTAG_FAKE_3		0x43
#define ZRTP_AUTHTAG_FAKE_4		0x44
#define ZRTP_AUTHTAG_FAKE_5		0x45

static int testAddMandatoryCryptoTypesIfNeeded(uint8_t algoType, uint8_t *algoTypes, uint8_t algoTypesCount, uint8_t *expectedTypes, uint8_t expectedTypesCount) {
	bzrtp_addMandatoryCryptoTypesIfNeeded(algoType, algoTypes, &algoTypesCount);
	return compareAlgoTypes(algoTypes, algoTypesCount, expectedTypes, expectedTypesCount);
}

struct st_add_crypto {
	uint8_t algoType;
	uint8_t algoTypes[7];
	uint8_t algoTypesCount;
	uint8_t expectedTypes[7];
	uint8_t expectedTypesCount;
};

static void test_addMandatoryCryptoTypesIfNeeded(void) {
	struct st_add_crypto crypto_types[] = {
		/* mandatory types */
		{ ZRTP_HASH_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_HASH_S256}, 1 },
		{ ZRTP_CIPHERBLOCK_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_CIPHER_AES1}, 1 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ ZRTP_KEYAGREEMENT_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_KEYAGREEMENT_DH3k}, 1 },
		{ ZRTP_KEYAGREEMENT_TYPE, {ZRTP_KEYAGREEMENT_X255}, 1, {ZRTP_KEYAGREEMENT_DH3k, ZRTP_KEYAGREEMENT_X255}, 2 },
		{ ZRTP_SAS_TYPE, {ZRTP_UNSET_ALGO}, 0, {ZRTP_SAS_B32}, 1 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 2, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 2 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64}, 2, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS32}, 3 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64}, 2, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 4 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3}, 5, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80}, 6, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 7 },
		/* overrride */
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4, ZRTP_AUTHTAG_FAKE_5}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_FAKE_4}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 7 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_FAKE_4, ZRTP_AUTHTAG_HS80}, 7, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_FAKE_1, ZRTP_AUTHTAG_FAKE_2, ZRTP_AUTHTAG_FAKE_3, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32}, 7 },
		/* uniqueness */
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64}, 4, {ZRTP_AUTHTAG_SK32, ZRTP_AUTHTAG_SK64, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 4 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS32}, 4, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ ZRTP_AUTHTAG_TYPE, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80, ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 4, {ZRTP_AUTHTAG_HS32, ZRTP_AUTHTAG_HS80}, 2 },
		{ 0, {ZRTP_UNSET_ALGO}, 0, {ZRTP_UNSET_ALGO}, 0, }
	};
	struct st_add_crypto *crypto_type;

	crypto_type = &crypto_types[0];
	while (crypto_type->algoTypesCount > 0) {
		BC_ASSERT_TRUE(testAddMandatoryCryptoTypesIfNeeded(crypto_type->algoType, crypto_type->algoTypes, crypto_type->algoTypesCount, crypto_type->expectedTypes, crypto_type->expectedTypesCount));
		crypto_type++;
	}
}

static test_t crypto_utils_tests[] = {
	TEST_NO_TAG("zrtpKDF", test_zrtpKDF),
	TEST_NO_TAG("CRC32", test_CRC32),
	TEST_NO_TAG("algo agreement", test_algoAgreement),
	TEST_NO_TAG("context algo setter and getter", test_algoSetterGetter),
	TEST_NO_TAG("adding mandatory crypto algorithms if needed", test_addMandatoryCryptoTypesIfNeeded)
};

test_suite_t crypto_utils_test_suite = {
	"Crypto Utils",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(crypto_utils_tests) / sizeof(crypto_utils_tests[0]),
	crypto_utils_tests
};
