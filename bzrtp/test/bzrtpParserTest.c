/**
 @file bzrtpParserTests.c
 @brief Test parsing and building ZRTP packet.

 @author Johan Pascal

 @copyright Copyright (C) 2014 Belledonne Communications, Grenoble, France
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "CUnit/Basic.h"

#include "bzrtp/bzrtp.h"
#include "typedef.h"
#include "packetParser.h"
#include "cryptoUtils.h"
#include "zidCache.h"
#include "testUtils.h"


/**
 * Test pattern : the packet data in the patternZRTPPackets, length, sequence number and SSRC in patternZRTPMetaData
 * Pattern generated from wireshark trace
 *
 */
#define TEST_PACKET_NUMBER 8
/* meta data: length, sequence number, SSRC */
static uint32_t patternZRTPMetaData[TEST_PACKET_NUMBER][3] = {
	{132, 24628, 0x5516868e},
	{28, 64911, 0x69473e6a},
	{132, 24632, 0x5516868e},
	{484, 24635, 0x5516868e},
	{484, 64916, 0x69473e6a},
	{92, 24636, 0x5516868e},
	{92, 64918, 0x69473e6a},
	{28, 24638, 0x5516868e}
};

static const uint8_t patternZRTPPackets[TEST_PACKET_NUMBER][512] = {
	/* This is a Hello packet, sequence number is 24628 */
	{0x10, 0x00, 0x60, 0x34, 0x5a, 0x52, 0x54, 0x50, 0x55, 0x16, 0x86, 0x8e, 0x50, 0x5a, 0x00, 0x1d, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x20, 0x20, 0x31, 0x2e, 0x31, 0x30, 0x4c, 0x49, 0x4e, 0x50, 0x48, 0x4f, 0x4e, 0x45, 0x2d, 0x5a, 0x52, 0x54, 0x50, 0x43, 0x50, 0x50, 0x93, 0x18, 0x6f, 0xb7, 0x3c, 0x39, 0xac, 0x3f, 0xc4, 0x32, 0xd2, 0xe5, 0x4d, 0x6a, 0xdb, 0x7c, 0x52, 0xfa, 0xd7, 0xba, 0xa0, 0xbd, 0x13, 0xa8, 0x23, 0xfa, 0x1f, 0x11, 0x5e, 0xe5, 0x78, 0xb6, 0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x00, 0x01, 0x12, 0x21, 0x53, 0x32, 0x35, 0x36, 0x41, 0x45, 0x53, 0x31, 0x48, 0x53, 0x33, 0x32, 0x48, 0x53, 0x38, 0x30, 0x44, 0x48, 0x33, 0x6b, 0x4d, 0x75, 0x6c, 0x74, 0x42, 0x33, 0x32, 0x20, 0x9a, 0xaa, 0xbb, 0x48, 0x50, 0x61, 0x5e, 0xa5, 0x67, 0xf9, 0xe6, 0xac},
	/* This is a Hello Ack packet, sequence number is 64911 */
	{0x10, 0x00, 0xfd, 0x8f, 0x5a, 0x52, 0x54, 0x50, 0x69, 0x47, 0x3e, 0x6a, 0x50, 0x5a, 0x00, 0x03, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x41, 0x43, 0x4b, 0x5f, 0x75, 0xcc, 0xe7},
	/* This is a Commit packet, sequence number is 24632 */
	{0x10, 0x00, 0x60, 0x38, 0x5a, 0x52, 0x54, 0x50, 0x55, 0x16, 0x86, 0x8e, 0x50, 0x5a, 0x00, 0x1d, 0x43, 0x6f, 0x6d, 0x6d, 0x69, 0x74, 0x20, 0x20, 0x76, 0xcc, 0x3b, 0xa3, 0x2b, 0x41, 0x86, 0x07, 0xa9, 0xc6, 0x98, 0x53, 0x7f, 0x4f, 0xe4, 0xb3, 0x21, 0xbd, 0x46, 0xd7, 0x4d, 0x08, 0xed, 0x5a, 0xac, 0xeb, 0x3a, 0xdb, 0x25, 0xfc, 0xb4, 0xef, 0xf0, 0x1f, 0x24, 0x02, 0x00, 0x00, 0x00, 0x00, 0x80, 0x24, 0xad, 0xfb, 0x53, 0x32, 0x35, 0x36, 0x41, 0x45, 0x53, 0x31, 0x48, 0x53, 0x33, 0x32, 0x44, 0x48, 0x33, 0x6b, 0x42, 0x33, 0x32, 0x20, 0x3f, 0x21, 0x6d, 0xc0, 0xb0, 0xb0, 0xb1, 0x6b, 0x78, 0x35, 0x64, 0x8d, 0xc4, 0xb5, 0x0f, 0xb6, 0x41, 0xc8, 0x0c, 0xc3, 0x65, 0x90, 0x37, 0x77, 0xe3, 0x16, 0xa7, 0xef, 0xcb, 0xbe, 0xc8, 0xdb, 0x27, 0x01, 0x5a, 0xe1, 0xe6, 0x04, 0x0e, 0xd0, 0xa3, 0x47, 0x6c, 0x8b},
	/* This is a DHPart1 packet, sequence number is 24635 */
	{0x10, 0x00, 0x60, 0x3b, 0x5a, 0x52, 0x54, 0x50, 0x55, 0x16, 0x86, 0x8e, 0x50, 0x5a, 0x00, 0x75, 0x44, 0x48, 0x50, 0x61, 0x72, 0x74, 0x31, 0x20, 0xfa, 0x1e, 0x37, 0xf8, 0x61, 0xe4, 0xc0, 0x0f, 0xc8, 0xdf, 0x76, 0x7f, 0xd1, 0x8d, 0xbb, 0x55, 0x0b, 0xd8, 0xce, 0x2d, 0x1f, 0xcc, 0xb4, 0xda, 0x15, 0x95, 0x88, 0x7f, 0xe3, 0xf3, 0x48, 0xc0, 0xfe, 0x51, 0x2e, 0xc5, 0xb1, 0x53, 0xef, 0xcd, 0xc1, 0x19, 0xcc, 0x8a, 0x27, 0x4b, 0x0a, 0x16, 0x1d, 0x4f, 0x63, 0x81, 0x78, 0x84, 0x68, 0x92, 0x36, 0xc4, 0xdc, 0x41, 0x2e, 0xc3, 0xaa, 0xde, 0xa8, 0x27, 0x32, 0x2e, 0x41, 0x62, 0x03, 0xf7, 0x7b, 0x5e, 0x50, 0x95, 0x30, 0x65, 0xe3, 0x47, 0xd7, 0x55, 0x39, 0x7c, 0xd7, 0x2c, 0x35, 0xa0, 0x28, 0x12, 0x81, 0x5f, 0x03, 0x48, 0x74, 0x2d, 0x3f, 0x57, 0x00, 0x46, 0x00, 0xe0, 0xcb, 0xb4, 0x49, 0x81, 0x22, 0x83, 0x61, 0x05, 0x74, 0xc9, 0x26, 0x88, 0x01, 0x5e, 0xbb, 0x69, 0x11, 0xa0, 0xbb, 0x6c, 0xe5, 0xe3, 0xe4, 0x72, 0x29, 0x8d, 0x3e, 0x58, 0x5f, 0xaf, 0x3f, 0x5b, 0x30, 0xc1, 0x04, 0x98, 0x2d, 0xbe, 0xb2, 0xf8, 0x24, 0xea, 0xb6, 0xf3, 0x0a, 0xa1, 0x3a, 0xf4, 0xe5, 0x1b, 0x31, 0xaa, 0xc7, 0x00, 0x32, 0x2b, 0x83, 0xd6, 0xc2, 0x92, 0x02, 0x43, 0x57, 0xd9, 0x65, 0x34, 0xe3, 0x94, 0x8c, 0x9d, 0x49, 0xdc, 0x75, 0xa7, 0xb0, 0x35, 0xb4, 0x2c, 0xb8, 0xdd, 0x36, 0xae, 0x18, 0x65, 0xda, 0x2d, 0xcd, 0x8d, 0x4d, 0x39, 0xa4, 0x5a, 0xbc, 0x82, 0x95, 0x89, 0x6c, 0x90, 0x90, 0x62, 0x2b, 0x12, 0x26, 0xd3, 0x42, 0x30, 0x87, 0x31, 0x21, 0xe0, 0x1f, 0x91, 0xb7, 0xd1, 0xf7, 0xe9, 0xa4, 0x6e, 0x9e, 0xdd, 0x52, 0x71, 0xda, 0x02, 0xb3, 0x2d, 0x7d, 0x34, 0x32, 0x1f, 0xc9, 0xde, 0xfb, 0x85, 0xa2, 0xb4, 0x2a, 0xd0, 0x7c, 0xb1, 0xb9, 0xd5, 0x87, 0x19, 0x7c, 0xfc, 0x40, 0xed, 0x78, 0x30, 0x2c, 0xcf, 0xec, 0x40, 0x56, 0xf6, 0x10, 0x3b, 0xf4, 0xde, 0x5a, 0x02, 0x77, 0x9c, 0xf9, 0xc3, 0x8c, 0xcb, 0xe9, 0x32, 0x3f, 0xe0, 0x30, 0x33, 0x09, 0xb3, 0xb8, 0x69, 0x76, 0x07, 0x3d, 0x57, 0xa4, 0x3f, 0x05, 0xfe, 0xe1, 0x67, 0x52, 0xab, 0xff, 0x43, 0x4f, 0xbb, 0xed, 0x26, 0xcf, 0x7b, 0x9a, 0x98, 0xac, 0xa3, 0xff, 0x8c, 0x2a, 0x2c, 0xdc, 0x6f, 0x60, 0x12, 0xa5, 0xca, 0x65, 0x0b, 0x43, 0xbf, 0x23, 0xb7, 0xee, 0x33, 0x27, 0xad, 0x3b, 0xb0, 0xfd, 0xfb, 0x4c, 0xf4, 0x16, 0xf7, 0x31, 0x5a, 0x11, 0x8d, 0xa6, 0x15, 0x36, 0xf2, 0x75, 0xb1, 0xdc, 0xe1, 0xe1, 0xf6, 0x41, 0x20, 0x08, 0x75, 0xbc, 0x91, 0xdc, 0x35, 0x2e, 0x10, 0x0a, 0x1c, 0xeb, 0x0b, 0x2b, 0xce, 0x27, 0xb7, 0x05, 0x70, 0x48, 0x36, 0xef, 0x50, 0x59, 0x90, 0xe3, 0xa8, 0x23, 0x6f, 0x7d, 0x99, 0xfa, 0x96, 0x24, 0x4e, 0xfb, 0x26, 0x60, 0x79, 0x23, 0x98, 0x18, 0x6b, 0xff, 0xdc, 0x37, 0xd4, 0xf2, 0x33, 0xe5, 0xfe, 0x68, 0xbb, 0xe3, 0x4c, 0xf0, 0x71, 0xb7, 0x55, 0xfb, 0xb2, 0xaa, 0x60, 0x16, 0xcd, 0x65, 0x34, 0x96, 0x71, 0xa2, 0x80, 0xac, 0x6b, 0x3c, 0x90, 0x72, 0xf1, 0xa5, 0x90, 0x03, 0xb6, 0x4f, 0x28, 0xe0, 0xd8, 0x3e, 0x20, 0x37, 0x53, 0xa2, 0x51, 0xa9, 0xac, 0x72, 0xe5, 0x4f, 0x9c, 0x8c, 0x5d, 0x68, 0xbe, 0x5c, 0xca, 0x91, 0x96, 0x96, 0xf6, 0xef, 0x0e, 0x1c, 0x55, 0xcb, 0x2f},
	/* This is a DHPart2 packet, sequence number is 64916 */
	{0x10, 0x00, 0xfd, 0x94, 0x5a, 0x52, 0x54, 0x50, 0x69, 0x47, 0x3e, 0x6a, 0x50, 0x5a, 0x00, 0x75, 0x44, 0x48, 0x50, 0x61, 0x72, 0x74, 0x32, 0x20, 0x5e, 0xae, 0x38, 0xb0, 0x80, 0xbf, 0x44, 0xd5, 0xf3, 0x09, 0x0c, 0xc1, 0x7b, 0xde, 0xca, 0x7c, 0xf5, 0x9f, 0x8f, 0x1f, 0xea, 0x64, 0xb8, 0x4c, 0xce, 0xb0, 0x82, 0xc8, 0xdb, 0x59, 0x8c, 0x7e, 0x55, 0xe6, 0x59, 0x90, 0x80, 0x43, 0x37, 0x5d, 0xd3, 0x04, 0x9e, 0x08, 0x76, 0x9e, 0x25, 0xf8, 0xb1, 0x07, 0xd7, 0xd8, 0xbb, 0x1e, 0xf4, 0x6d, 0x00, 0xeb, 0x74, 0xc1, 0x05, 0x54, 0xe5, 0xa0, 0x78, 0xa5, 0x53, 0x89, 0xa0, 0x95, 0x92, 0xc6, 0x70, 0x72, 0x67, 0xd9, 0x38, 0x35, 0x18, 0x57, 0xd1, 0xfa, 0x83, 0xd7, 0x71, 0x29, 0x61, 0xa9, 0x85, 0xac, 0xf8, 0xa6, 0xb2, 0x86, 0x21, 0x0e, 0xba, 0xf2, 0x2c, 0x26, 0x8f, 0x1c, 0xbb, 0x02, 0xa5, 0x38, 0xc1, 0xe4, 0x3b, 0x2b, 0xb4, 0xad, 0x66, 0x48, 0x11, 0x76, 0xd8, 0x02, 0xa1, 0xa1, 0xaf, 0xd5, 0x1e, 0xe6, 0x5a, 0xf7, 0xc0, 0xd0, 0x7a, 0xf5, 0xa0, 0xb0, 0x86, 0x3e, 0x27, 0xc6, 0x2c, 0x68, 0x16, 0x72, 0xd4, 0x45, 0x74, 0xd1, 0xf8, 0x62, 0x9c, 0x63, 0xca, 0x7e, 0x7b, 0x05, 0xb4, 0x12, 0x8d, 0xcc, 0xf8, 0x9f, 0xea, 0x16, 0x0e, 0x8b, 0x28, 0x89, 0x26, 0x01, 0x86, 0x32, 0x1a, 0x43, 0x38, 0xa5, 0x39, 0x85, 0x4c, 0xff, 0xc3, 0x82, 0xa2, 0x22, 0x55, 0xce, 0x31, 0x11, 0x84, 0x7f, 0xb8, 0xeb, 0xc8, 0x19, 0xb7, 0x11, 0xa5, 0xba, 0x0f, 0x70, 0x72, 0xf5, 0x42, 0xbd, 0x88, 0x85, 0x87, 0x4a, 0xe1, 0x2f, 0x04, 0xb3, 0x7c, 0x29, 0x39, 0xbb, 0x63, 0xb1, 0x14, 0x23, 0xc4, 0xbe, 0x7b, 0x2e, 0x9f, 0x25, 0x5e, 0x47, 0x14, 0x91, 0x9c, 0xcc, 0xb7, 0x67, 0x73, 0x69, 0x23, 0x11, 0x7a, 0x7c, 0x85, 0x9a, 0x36, 0xc2, 0xd5, 0xb7, 0x35, 0x75, 0x10, 0xb7, 0x53, 0x11, 0xd7, 0x09, 0x95, 0xcf, 0x27, 0x5b, 0x6b, 0x5b, 0xa1, 0xcf, 0x00, 0x47, 0x44, 0x4a, 0x00, 0xbc, 0xc5, 0x0d, 0x07, 0x3b, 0x61, 0xdb, 0xba, 0x72, 0xd9, 0xf5, 0xf6, 0x0e, 0xb8, 0x5e, 0x4e, 0xcf, 0xc0, 0x72, 0x55, 0x01, 0xff, 0x48, 0x3c, 0x06, 0x46, 0xa2, 0xb9, 0x92, 0xc1, 0xe1, 0x40, 0x89, 0xd9, 0x69, 0x80, 0xa2, 0x63, 0xdb, 0x09, 0x4f, 0x56, 0xf7, 0xad, 0x9a, 0x52, 0x93, 0x0d, 0x09, 0xed, 0x40, 0x0a, 0x24, 0x35, 0x3f, 0x0b, 0x37, 0x71, 0x04, 0xbc, 0xda, 0x07, 0x46, 0x63, 0xca, 0x04, 0x47, 0xe7, 0xc4, 0xcb, 0x4a, 0x2f, 0x39, 0x7b, 0xe9, 0x8b, 0x67, 0x10, 0xd4, 0x8d, 0xfc, 0x97, 0xe6, 0x93, 0x8c, 0xd9, 0x88, 0x7c, 0x54, 0x3d, 0xe4, 0x23, 0x4b, 0xb3, 0xb1, 0xeb, 0x03, 0x69, 0xf3, 0x5b, 0x4e, 0x66, 0x89, 0xd2, 0x7a, 0x3a, 0x12, 0x77, 0x08, 0xab, 0xa2, 0xfa, 0xa6, 0x6f, 0x88, 0xc2, 0x02, 0xa4, 0xc1, 0x46, 0x2f, 0x3d, 0xed, 0x25, 0x7d, 0x0d, 0xe4, 0xde, 0x40, 0xb5, 0x93, 0xb5, 0xc5, 0xfa, 0x52, 0x42, 0xab, 0xdf, 0x02, 0x9c, 0x42, 0x15, 0xab, 0x46, 0x8e, 0x70, 0x48, 0x59, 0xf4, 0x04, 0x3f, 0x46, 0xac, 0xdf, 0xdc, 0xdc, 0x40, 0x4a, 0x94, 0xc3, 0x01, 0x73, 0xca, 0x1d, 0xd0, 0x2a, 0x85, 0x75, 0xa4, 0x62, 0x75, 0xee, 0xf5, 0xa5, 0xcd, 0x2c, 0x46, 0x19, 0x75, 0x08, 0x25, 0xc8, 0x7f, 0x9d, 0x52, 0x61, 0xf4, 0x31, 0xe7, 0x57, 0x2e, 0x58, 0x69, 0x49, 0x7d, 0xe6, 0xc1},
	/* This is a Confirm1 packet, sequence number is 24636 */
	{0x10, 0x00, 0x60, 0x3c, 0x5a, 0x52, 0x54, 0x50, 0x55, 0x16, 0x86, 0x8e, 0x50, 0x5a, 0x00, 0x13, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x72, 0x6d, 0x31, 0xa0, 0x71, 0x1b, 0xe5, 0x76, 0x7c, 0x8a, 0x26, 0x55, 0xfc, 0x71, 0x0c, 0x12, 0x75, 0xc7, 0x69, 0x19, 0xac, 0x41, 0xb1, 0xd3, 0x46, 0x7e, 0xb4, 0xcb, 0x99, 0x4e, 0x9a, 0xe1, 0x98, 0x9d, 0x72, 0xc6, 0xa5, 0x52, 0xd7, 0x56, 0x99, 0xf9, 0xcc, 0x02, 0x45, 0x0c, 0xc9, 0xb9, 0x64, 0x49, 0x12, 0x7c, 0x07, 0xbe, 0x1b, 0xf8, 0x0f, 0xba, 0x98, 0xcc, 0x5a, 0xd0, 0x12, 0x75, 0xc6, 0x53, 0x20, 0x7f, 0xe4, 0x6f, 0x82},
	/* This is a Confirm2 packet, sequence number is 64918 */
	{0x10, 0x00, 0xfd, 0x96, 0x5a, 0x52, 0x54, 0x50, 0x69, 0x47, 0x3e, 0x6a, 0x50, 0x5a, 0x00, 0x13, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x72, 0x6d, 0x32, 0xb6, 0xb8, 0x0b, 0xbc, 0x55, 0x71, 0x64, 0xbf, 0x02, 0xe4, 0x49, 0x57, 0x51, 0xea, 0xc6, 0x5f, 0x25, 0x78, 0xbb, 0x79, 0xc8, 0x8f, 0xd5, 0x6b, 0x17, 0x93, 0x06, 0x9c, 0x21, 0x51, 0xa4, 0xde, 0x27, 0x01, 0x03, 0x75, 0x83, 0x58, 0x3d, 0xa6, 0xdb, 0x66, 0x64, 0xce, 0xfd, 0x19, 0x16, 0x21, 0x31, 0x12, 0xcd, 0x6f, 0xf3, 0x28, 0x08, 0x0a, 0x92, 0x58, 0xbd, 0x65, 0x53, 0x04, 0x04, 0x55, 0xa2, 0x29, 0x88, 0x1f},
	/* This is a Conf2Ack packet, sequence number is 24638 */
	{0x10, 0x00, 0x60, 0x3e, 0x5a, 0x52, 0x54, 0x50, 0x55, 0x16, 0x86, 0x8e, 0x50, 0x5a, 0x00, 0x03, 0x43, 0x6f, 0x6e, 0x66, 0x32, 0x41, 0x43, 0x4b, 0xc6, 0x41, 0xb2, 0x1e}
};

/* Hash images for both sides */
uint8_t H5516868e[4][32] = {
	{},
	{0xfa, 0x1e, 0x37, 0xf8, 0x61, 0xe4, 0xc0, 0x0f, 0xc8, 0xdf, 0x76, 0x7f, 0xd1, 0x8d, 0xbb, 0x55, 0x0b, 0xd8, 0xce, 0x2d, 0x1f, 0xcc, 0xb4, 0xda, 0x15, 0x95, 0x88, 0x7f, 0xe3, 0xf3, 0x48, 0xc0},
	{0x76, 0xcc, 0x3b, 0xa3, 0x2b, 0x41, 0x86, 0x07, 0xa9, 0xc6, 0x98, 0x53, 0x7f, 0x4f, 0xe4, 0xb3, 0x21, 0xbd, 0x46, 0xd7, 0x4d, 0x08, 0xed, 0x5a, 0xac, 0xeb, 0x3a, 0xdb, 0x25, 0xfc, 0xb4, 0xef},
	{0x93, 0x18, 0x6f, 0xb7, 0x3c, 0x39, 0xac, 0x3f, 0xc4, 0x32, 0xd2, 0xe5, 0x4d, 0x6a, 0xdb, 0x7c, 0x52, 0xfa, 0xd7, 0xba, 0xa0, 0xbd, 0x13, 0xa8, 0x23, 0xfa, 0x1f, 0x11, 0x5e, 0xe5, 0x78, 0xb6}
};


void test_parser(void) {
	int i, retval;
	bzrtpPacket_t *zrtpPacket;
	
	/* Create zrtp Context to use H0-H3 chains */
	bzrtpContext_t *context69473e6a = bzrtp_createBzrtpContext();
	bzrtpContext_t *context5516868e = bzrtp_createBzrtpContext();


	/* replace created H by the patterns one to be able to generate the correct packet */
	memcpy (context5516868e->selfH[1], H5516868e[1], 32);
	memcpy (context5516868e->selfH[2], H5516868e[2], 32);
	memcpy (context5516868e->selfH[3], H5516868e[3], 32);

	/* preset the key agreement algo in the contexts */
	context69473e6a->keyAgreementAlgo = ZRTP_KEYAGREEMENT_DH3k;
	context5516868e->keyAgreementAlgo = ZRTP_KEYAGREEMENT_DH3k;
	context69473e6a->cipherAlgo = ZRTP_CIPHER_AES1;
	context5516868e->cipherAlgo = ZRTP_CIPHER_AES1;
	context69473e6a->hashAlgo = ZRTP_HASH_S256;
	context5516868e->hashAlgo = ZRTP_HASH_S256;

	updateCryptoFunctionPointers(context69473e6a);
	updateCryptoFunctionPointers(context5516868e);
	
	/* set some keys for mac and ciphering, they are not valid so we shall have an error on the mac verification */
	uint8_t fakeMacKeyi[32] = { 0x0b, 0xd8, 0xce, 0x2d, 0x1f, 0xcc, 0xb4, 0xda, 0x15, 0x95, 0x88, 0x7f, 0xe3, 0xf3, 0x48, 0xc0, 0x93, 0x18, 0x6f, 0xb7, 0x3c, 0x39, 0xac, 0x3f, 0xc4, 0x32, 0xd2, 0xe5, 0x4d, 0x6a, 0xdb, 0x7c};
	uint8_t fakeMacKeyr[32] = { 0x0a, 0xde, 0xce, 0x2d, 0x1f, 0xcc, 0xb4, 0xda, 0x15, 0x95, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x93, 0x18, 0x6f, 0xb7, 0x3c, 0x39, 0xac, 0x3f, 0xc4, 0x32, 0xd2, 0xe5, 0x4d, 0x6a, 0xdb, 0x7c};
	uint8_t fakeKeyi[16] = { 0xaa, 0xaa, 0xce, 0x2d, 0x1f, 0xcc, 0xb4, 0xda, 0x15, 0x95, 0x88, 0x7f, 0xe3, 0xf3, 0x48, 0xc0};
	uint8_t fakeKeyr[16] = { 0xba, 0xba, 0xce, 0x2d, 0x1f, 0xcc, 0xb4, 0xda, 0x15, 0x95, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
	context69473e6a->mackeyi = (uint8_t *)malloc(32);
	context5516868e->mackeyi = (uint8_t *)malloc(32);
	context69473e6a->mackeyr = (uint8_t *)malloc(32);
	context5516868e->mackeyr = (uint8_t *)malloc(32);

	context69473e6a->zrtpkeyi = (uint8_t *)malloc(16);
	context5516868e->zrtpkeyi = (uint8_t *)malloc(16);
	context69473e6a->zrtpkeyr = (uint8_t *)malloc(16);
	context5516868e->zrtpkeyr = (uint8_t *)malloc(16);

	memcpy(context69473e6a->mackeyi, fakeMacKeyi, 32);
	memcpy(context5516868e->mackeyi, fakeMacKeyi, 32);
	memcpy(context5516868e->mackeyr, fakeMacKeyr, 32);
	memcpy(context69473e6a->mackeyr, fakeMacKeyr, 32);

	memcpy(context69473e6a->zrtpkeyr, fakeKeyr, 16);
	memcpy(context5516868e->zrtpkeyr, fakeKeyr, 16);
	memcpy(context69473e6a->zrtpkeyi, fakeKeyi, 16);
	memcpy(context5516868e->zrtpkeyi, fakeKeyi, 16);

	for (i=0; i<TEST_PACKET_NUMBER; i++) {
		/* parse a packet string from patterns */
		zrtpPacket = bzrtp_packetCheck(patternZRTPPackets[i], patternZRTPMetaData[i][0], (patternZRTPMetaData[i][1])-1, &retval);
		retval +=  bzrtp_packetParser((patternZRTPMetaData[i][2]==0x5516868e)?context5516868e:context69473e6a, patternZRTPPackets[i], patternZRTPMetaData[i][0], zrtpPacket);
		printf("parsing Ret val is %x index is %d\n", retval, i);
		/* free the packet string as will be created again by the packetBuild function and might have been copied by packetParser */
		free(zrtpPacket->packetString);
		/* build a packet string from the parser packet*/
		retval = bzrtp_packetBuild((patternZRTPMetaData[i][2]==0x5516868e)?context5516868e:context69473e6a, zrtpPacket, patternZRTPMetaData[i][1]);
		if (retval ==0) {
			packetDump(zrtpPacket, 1);
		} else {
			printf("Ret val is %x index is %d\n", retval, i);
		}

		/* check they are the same */
		if (zrtpPacket->packetString != NULL) {
			CU_ASSERT_TRUE(memcmp(zrtpPacket->packetString, patternZRTPPackets[i], patternZRTPMetaData[i][0]) == 0);
		}
		bzrtp_freeZrtpPacket(zrtpPacket);
	}

	bzrtp_destroyBzrtpContext(context69473e6a);
	bzrtp_destroyBzrtpContext(context5516868e);


}


static FILE *ALICECACHE;
static FILE *BOBCACHE;
void initCacheAlice(char *filename) {
	ALICECACHE = fopen(filename, "r+");
	rewind(ALICECACHE);
}
void initCacheBob(char *filename) {
	BOBCACHE = fopen(filename, "r+");	
	rewind(BOBCACHE);
}

int freadAlice(uint8_t *output, uint16_t size) {
	return fread(output, 1, size, ALICECACHE);
}

int fwriteAlice(uint8_t *input, uint16_t size) {
	return fwrite(input, 1, size, ALICECACHE);
}

int fgetPosAlice(long *position) {
	*position = ftell(ALICECACHE);
	return 0;
}

int fsetPosAlice(long position) {
	return fseek(ALICECACHE, position, SEEK_SET);
}

int freadBob(uint8_t *output, uint16_t size) {
	return fread(output, 1, size, BOBCACHE);
}

int fwriteBob(uint8_t *input, uint16_t size) {
	return fwrite(input, 1, size, BOBCACHE);
}

int fgetPosBob(long *position) {
	*position = ftell(BOBCACHE);
	return 0;
}

int fsetPosBob(long position) {
	return fseek(BOBCACHE, position, SEEK_SET);
}

void test_parserComplete() {

	int retval;
	/* alice's maintained packet */
	bzrtpPacket_t *alice_Hello, *alice_HelloFromBob, *alice_HelloACK, *alice_HelloACKFromBob;
	/* bob's maintained packet */
	bzrtpPacket_t *bob_Hello, *bob_HelloFromAlice, *bob_HelloACK, *bob_HelloACKFromAlice;
	/* Create zrtp Context */
	bzrtpContext_t *contextAlice = bzrtp_createBzrtpContext();
	bzrtpContext_t *contextBob = bzrtp_createBzrtpContext();

	/* create the cache files */
	initCacheAlice("test/ZIDAlice.txt");
	initCacheBob("test/ZIDBob.txt");
	if (ALICECACHE==NULL) {
		printf("Error opening Alice %s\n", strerror(errno));
	}
	bzrtp_setCallback(contextAlice, (int (*)())freadAlice, ZRTP_CALLBACK_READCACHE);
	bzrtp_setCallback(contextAlice, (int (*)())fwriteAlice, ZRTP_CALLBACK_WRITECACHE);
	bzrtp_setCallback(contextAlice, (int (*)())fsetPosAlice, ZRTP_CALLBACK_SETCACHEPOSITION);
	bzrtp_setCallback(contextAlice, (int (*)())fgetPosAlice, ZRTP_CALLBACK_GETCACHEPOSITION);

	bzrtp_setCallback(contextBob, (int (*)())freadBob, ZRTP_CALLBACK_READCACHE);
	bzrtp_setCallback(contextBob, (int (*)())fwriteBob, ZRTP_CALLBACK_WRITECACHE);
	bzrtp_setCallback(contextBob, (int (*)())fsetPosBob, ZRTP_CALLBACK_SETCACHEPOSITION);
	bzrtp_setCallback(contextBob, (int (*)())fgetPosBob, ZRTP_CALLBACK_GETCACHEPOSITION);

	printf ("Init the contexts\n\n");
	/* end the context init */
	bzrtp_initBzrtpContext(contextAlice);
	bzrtp_initBzrtpContext(contextBob);

	/* now create Alice and BOB Hello packet */
	alice_Hello = bzrtp_createZrtpPacket(contextAlice, MSGTYPE_HELLO, 0x12345678, &retval);
	if (bzrtp_packetBuild(contextAlice, alice_Hello, contextAlice->selfSequenceNumber) ==0) {
		contextAlice->selfSequenceNumber++;
		contextAlice->selfPackets[HELLO_MESSAGE_STORE_ID] = alice_Hello;
	}
	bob_Hello = bzrtp_createZrtpPacket(contextBob, MSGTYPE_HELLO, 0x87654321, &retval);
	if (bzrtp_packetBuild(contextBob, bob_Hello, contextBob->selfSequenceNumber) ==0) {
		contextBob->selfSequenceNumber++;
		contextBob->selfPackets[HELLO_MESSAGE_STORE_ID] = bob_Hello;
	}

	/* now send Alice Hello's to Bob and vice-versa, so they parse them */
	alice_HelloFromBob = bzrtp_packetCheck(bob_Hello->packetString, bob_Hello->messageLength+16, contextAlice->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextAlice, bob_Hello->packetString, bob_Hello->messageLength+16, alice_HelloFromBob);
	printf ("Alice parsing returns %x\n", retval);
	if (retval==0) {
		contextAlice->peerSequenceNumber = alice_HelloFromBob->sequenceNumber;
		/* save bob's Hello packet in Alice's context */
		contextAlice->peerPackets[HELLO_MESSAGE_STORE_ID] = alice_HelloFromBob;

		/* determine crypto Algo to use */
		bzrtpHelloMessage_t *alice_HelloFromBob_message = (bzrtpHelloMessage_t *)alice_HelloFromBob->messageData;
		retval = crypoAlgoAgreement(contextAlice, contextAlice->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData);
		if (retval == 0) {
			printf ("Alice selected algo %x\n", contextAlice->keyAgreementAlgo);
			memcpy(contextAlice->peerZID, alice_HelloFromBob_message->ZID, 12);
		}
	}

	bob_HelloFromAlice = bzrtp_packetCheck(alice_Hello->packetString, alice_Hello->messageLength+16, contextBob->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextBob, alice_Hello->packetString, alice_Hello->messageLength+16, bob_HelloFromAlice);
	printf ("Bob parsing returns %x\n", retval);
	if (retval==0) {
		contextBob->peerSequenceNumber = bob_HelloFromAlice->sequenceNumber;
		/* save alice's Hello packet in bob's context */
		contextBob->peerPackets[HELLO_MESSAGE_STORE_ID] = bob_HelloFromAlice;

		/* determine crypto Algo to use */
		bzrtpHelloMessage_t *bob_HelloFromAlice_message = (bzrtpHelloMessage_t *)bob_HelloFromAlice->messageData;
		retval = crypoAlgoAgreement(contextBob, contextBob->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData);
		if (retval == 0) {
			printf ("Bob selected algo %x\n", contextBob->keyAgreementAlgo);
			memcpy(contextBob->peerZID, bob_HelloFromAlice_message->ZID, 12);
		}

	}

	/* update context with hello message information : H3  and compute initiator and responder's shared secret Hashs */
	bzrtpHelloMessage_t *alice_HelloFromBob_message = (bzrtpHelloMessage_t *)alice_HelloFromBob->messageData;
	memcpy(contextAlice->peerH[3], alice_HelloFromBob_message->H3, 32);
	bzrtpHelloMessage_t *bob_HelloFromAlice_message = (bzrtpHelloMessage_t *)bob_HelloFromAlice->messageData;
	memcpy(contextBob->peerH[3], bob_HelloFromAlice_message->H3, 32);

	/* get the secrets associated to peer ZID */
	getPeerAssociatedSecretsHash(contextAlice, alice_HelloFromBob_message->ZID);
	getPeerAssociatedSecretsHash(contextBob, bob_HelloFromAlice_message->ZID);

	/* compute the initiator hashed secret as in rfc section 4.3.1 */
	if (contextAlice->cachedSecret.rs1!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.rs1, contextAlice->cachedSecret.rs1Length, (uint8_t *)"Initiator", 9, 8, contextAlice->initiatorCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.rs1ID, 8);
	}

	if (contextAlice->cachedSecret.rs1!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.rs1, contextAlice->cachedSecret.rs1Length, (uint8_t *)"Initiator", 9, 8, contextAlice->initiatorCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.rs1ID, 8);
	}

	if (contextAlice->cachedSecret.rs2!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.rs2, contextAlice->cachedSecret.rs2Length, (uint8_t *)"Initiator", 9, 8, contextAlice->initiatorCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.rs2ID, 8);
	}

	if (contextAlice->cachedSecret.auxsecret!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.auxsecret, contextAlice->cachedSecret.auxsecretLength, contextAlice->selfH[3], 32, 8, contextAlice->initiatorCachedSecretHash.auxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.auxsecretID, 8);
	}

	if (contextAlice->cachedSecret.pbxsecret!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.pbxsecret, contextAlice->cachedSecret.pbxsecretLength, (uint8_t *)"Initiator", 9, 8, contextAlice->initiatorCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.pbxsecretID, 8);
	}

	if (contextAlice->cachedSecret.rs1!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.rs1, contextAlice->cachedSecret.rs1Length, (uint8_t *)"Responder", 9, 8, contextAlice->responderCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->responderCachedSecretHash.rs1ID, 8);
	}

	if (contextAlice->cachedSecret.rs2!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.rs2, contextAlice->cachedSecret.rs2Length, (uint8_t *)"Responder", 9, 8, contextAlice->responderCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->responderCachedSecretHash.rs2ID, 8);
	}

	if (contextAlice->cachedSecret.auxsecret!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.auxsecret, contextAlice->cachedSecret.auxsecretLength, contextAlice->peerH[3], 32, 8, contextAlice->responderCachedSecretHash.auxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->responderCachedSecretHash.auxsecretID, 8);
	}

	if (contextAlice->cachedSecret.pbxsecret!=NULL) {
		contextAlice->hmacFunction(contextAlice->cachedSecret.pbxsecret, contextAlice->cachedSecret.pbxsecretLength, (uint8_t *)"Responder", 9, 8, contextAlice->responderCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->responderCachedSecretHash.pbxsecretID, 8);
	}


	/* Bob hashes*/
	if (contextBob->cachedSecret.rs1!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.rs1, contextBob->cachedSecret.rs1Length, (uint8_t *)"Initiator", 9, 8, contextBob->initiatorCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.rs1ID, 8);
	}

	if (contextBob->cachedSecret.rs1!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.rs1, contextBob->cachedSecret.rs1Length, (uint8_t *)"Initiator", 9, 8, contextBob->initiatorCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.rs1ID, 8);
	}

	if (contextBob->cachedSecret.rs2!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.rs2, contextBob->cachedSecret.rs2Length, (uint8_t *)"Initiator", 9, 8, contextBob->initiatorCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.rs2ID, 8);
	}

	if (contextBob->cachedSecret.auxsecret!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.auxsecret, contextBob->cachedSecret.auxsecretLength, contextBob->selfH[3], 32, 8, contextBob->initiatorCachedSecretHash.auxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.auxsecretID, 8);
	}

	if (contextBob->cachedSecret.pbxsecret!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.pbxsecret, contextBob->cachedSecret.pbxsecretLength, (uint8_t *)"Initiator", 9, 8, contextBob->initiatorCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.pbxsecretID, 8);
	}

	if (contextBob->cachedSecret.rs1!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.rs1, contextBob->cachedSecret.rs1Length, (uint8_t *)"Responder", 9, 8, contextBob->responderCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->responderCachedSecretHash.rs1ID, 8);
	}

	if (contextBob->cachedSecret.rs2!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.rs2, contextBob->cachedSecret.rs2Length, (uint8_t *)"Responder", 9, 8, contextBob->responderCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->responderCachedSecretHash.rs2ID, 8);
	}

	if (contextBob->cachedSecret.auxsecret!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.auxsecret, contextBob->cachedSecret.auxsecretLength, contextBob->peerH[3], 32, 8, contextBob->responderCachedSecretHash.auxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->responderCachedSecretHash.auxsecretID, 8);
	}

	if (contextBob->cachedSecret.pbxsecret!=NULL) {
		contextBob->hmacFunction(contextBob->cachedSecret.pbxsecret, contextBob->cachedSecret.pbxsecretLength, (uint8_t *)"Responder", 9, 8, contextBob->responderCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->responderCachedSecretHash.pbxsecretID, 8);
	}

	/* dump alice's packet on both sides */
	printf ("\nAlice original Packet is \n");
	packetDump(alice_Hello, 1);
	printf ("\nBob's parsed Alice Packet is \n");
	packetDump(bob_HelloFromAlice, 0);

	/* Create the DHPart2 packet (that we then may change to DHPart1 if we ended to be the responder) */
	bzrtpPacket_t *alice_selfDHPart = bzrtp_createZrtpPacket(contextAlice, MSGTYPE_DHPART2, 0x12345678, &retval); 
	retval += bzrtp_packetBuild(contextAlice, alice_selfDHPart, 0); /* we don't care now about sequence number as we just need to build the message to be able to insert a hash of it into the commit packet */
	if (retval == 0) { /* ok, insert it in context as we need it to build the commit packet */
		contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID] = alice_selfDHPart;
	} else {
		printf ("Alice building DHPart packet returns %x\n", retval);
	}
	bzrtpPacket_t *bob_selfDHPart = bzrtp_createZrtpPacket(contextBob, MSGTYPE_DHPART2, 0x87654321, &retval); 
	retval +=bzrtp_packetBuild(contextBob, bob_selfDHPart, 0); /* we don't care now about sequence number as we just need to build the message to be able to insert a hash of it into the commit packet */
	if (retval == 0) { /* ok, insert it in context as we need it to build the commit packet */
		contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID] = bob_selfDHPart;
	} else {
		printf ("Bob building DHPart packet returns %x\n", retval);
	}
	printf("Alice DHPart packet:\n");
	packetDump(alice_selfDHPart,0);
	printf("Bob DHPart packet:\n");
	packetDump(bob_selfDHPart,0);

	/* respond to HELLO packet with an HelloACK - 1 create packets */
	alice_HelloACK = bzrtp_createZrtpPacket(contextAlice, MSGTYPE_HELLOACK, 0x12345678, &retval);
	retval += bzrtp_packetBuild(contextAlice, alice_HelloACK, contextAlice->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->selfSequenceNumber++;
	} else {
		printf("Alice building HelloACK return %x\n", retval);
	}

	bob_HelloACK = bzrtp_createZrtpPacket(contextBob, MSGTYPE_HELLOACK, 0x87654321, &retval);
	retval += bzrtp_packetBuild(contextBob, bob_HelloACK, contextBob->selfSequenceNumber);
	if (retval == 0) {
		contextBob->selfSequenceNumber++;
	} else {
		printf("Bob building HelloACK return %x\n", retval);
	}

	/* exchange the HelloACK */
	alice_HelloACKFromBob = bzrtp_packetCheck(bob_HelloACK->packetString, bob_HelloACK->messageLength+16, contextAlice->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextAlice, bob_HelloACK->packetString, bob_HelloACK->messageLength+16, alice_HelloACKFromBob);
	printf ("Alice parsing Hello ACK returns %x\n", retval);
	if (retval==0) {
		contextAlice->peerSequenceNumber = alice_HelloACKFromBob->sequenceNumber;
	}

	bob_HelloACKFromAlice = bzrtp_packetCheck(alice_HelloACK->packetString, alice_HelloACK->messageLength+16, contextBob->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextBob, alice_HelloACK->packetString, alice_HelloACK->messageLength+16, bob_HelloACKFromAlice);
	printf ("Bob parsing Hello ACK returns %x\n", retval);
	if (retval==0) {
		contextBob->peerSequenceNumber = bob_HelloACKFromAlice->sequenceNumber;
	}


	/* now build the commit message (both Alice and Bob will send it, then use the mechanism of rfc section 4.2 to determine who will be the initiator)*/
	bzrtpPacket_t *alice_Commit = bzrtp_createZrtpPacket(contextAlice, MSGTYPE_COMMIT, 0x12345678, &retval);
	retval += bzrtp_packetBuild(contextAlice, alice_Commit, contextAlice->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->selfSequenceNumber++;
		contextAlice->selfPackets[COMMIT_MESSAGE_STORE_ID] = alice_Commit;
	}
	printf("Alice building Commit return %x\n", retval);
	
	bzrtpPacket_t *bob_Commit = bzrtp_createZrtpPacket(contextBob, MSGTYPE_COMMIT, 0x87654321, &retval);
	retval += bzrtp_packetBuild(contextBob, bob_Commit, contextBob->selfSequenceNumber);
	if (retval == 0) {
		contextBob->selfSequenceNumber++;
		contextBob->selfPackets[COMMIT_MESSAGE_STORE_ID] = bob_Commit;
	}
	printf("Bob building Commit return %x\n", retval);


	/* and exchange the commits */
	bzrtpPacket_t *bob_CommitFromAlice = bzrtp_packetCheck(alice_Commit->packetString, alice_Commit->messageLength+16, contextBob->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextBob, alice_Commit->packetString, alice_Commit->messageLength+16, bob_CommitFromAlice);
	printf ("Bob parsing Commit returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		bzrtpCommitMessage_t *bob_CommitFromAlice_message = (bzrtpCommitMessage_t *)bob_CommitFromAlice->messageData;
		contextBob->peerSequenceNumber = bob_CommitFromAlice->sequenceNumber;
		memcpy(contextBob->peerH[2], bob_CommitFromAlice_message->H2, 32);
		contextBob->peerPackets[COMMIT_MESSAGE_STORE_ID] = bob_CommitFromAlice;
	}
	packetDump(bob_CommitFromAlice, 0);

	bzrtpPacket_t *alice_CommitFromBob = bzrtp_packetCheck(bob_Commit->packetString, bob_Commit->messageLength+16, contextAlice->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, bob_Commit->packetString, bob_Commit->messageLength+16, alice_CommitFromBob);
	printf ("Alice parsing Commit returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		bzrtpCommitMessage_t *alice_CommitFromBob_message = (bzrtpCommitMessage_t *)alice_CommitFromBob->messageData;
		contextAlice->peerSequenceNumber = alice_CommitFromBob->sequenceNumber;
		memcpy(contextAlice->peerH[2], alice_CommitFromBob_message->H2, 32);
		contextAlice->peerPackets[COMMIT_MESSAGE_STORE_ID] = alice_CommitFromBob;
	}
	packetDump(alice_CommitFromBob, 0);

	/* Now determine who shall be the initiator : rfc section 4.2 */
	/* select the one with the lowest value of hvi */
	/* for test purpose, we will set Alice as the initiator */
	contextBob->role = RESPONDER;

	/* Bob (responder) shall update his selected algo list to match Alice selection */
	/* no need to do this here as we have the same selection */

	/* Bob is the responder, rebuild his DHPart packet to be responder and not initiator : */
	/* as responder, bob must also swap his aux shared secret between responder and initiator as they are computed using the H3 and not a string */
	uint8_t tmpBuffer[8];
	memcpy(tmpBuffer, contextBob->initiatorCachedSecretHash.auxsecretID, 8);
	memcpy(contextBob->initiatorCachedSecretHash.auxsecretID, contextBob->responderCachedSecretHash.auxsecretID, 8);
	memcpy(contextBob->responderCachedSecretHash.auxsecretID, tmpBuffer, 8);

	contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageType = MSGTYPE_DHPART1; /* we are now part 1*/
	bzrtpDHPartMessage_t *bob_DHPart1 = (bzrtpDHPartMessage_t *)contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageData;
	/* change the shared secret ID to the responder one (we set them by default to the initiator's one) */
	memcpy(bob_DHPart1->rs1ID, contextBob->responderCachedSecretHash.rs1ID, 8);
	memcpy(bob_DHPart1->rs2ID, contextBob->responderCachedSecretHash.rs2ID, 8);
	memcpy(bob_DHPart1->auxsecretID, contextBob->responderCachedSecretHash.auxsecretID, 8);
	memcpy(bob_DHPart1->pbxsecretID, contextBob->responderCachedSecretHash.pbxsecretID, 8);

	retval +=bzrtp_packetBuild(contextBob, contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID],contextBob->selfSequenceNumber);
	if (retval == 0) {
		contextBob->selfSequenceNumber++;
	}
	printf("Bob building DHPart1 return %x\n", retval);


	/* Alice parse bob's DHPart1 message */
	bzrtpPacket_t *alice_DHPart1FromBob = bzrtp_packetCheck(contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->packetString, contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength+16, contextAlice->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->packetString, contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength+16, alice_DHPart1FromBob);
	printf ("Alice parsing DHPart1 returns %x\n", retval);
	bzrtpDHPartMessage_t *alice_DHPart1FromBob_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->peerSequenceNumber = alice_DHPart1FromBob->sequenceNumber;
		alice_DHPart1FromBob_message = (bzrtpDHPartMessage_t *)alice_DHPart1FromBob->messageData;
		memcpy(contextAlice->peerH[1], alice_DHPart1FromBob_message->H1, 32);
		contextAlice->peerPackets[DHPART2_MESSAGE_STORE_ID] = alice_DHPart1FromBob;
	}
	packetDump(alice_DHPart1FromBob, 1);

	/* Now Alice may check which shared secret she expected and if they are valid in bob's DHPart1 */
	if (contextAlice->cachedSecret.rs1!=NULL) {
		if (memcmp(contextAlice->responderCachedSecretHash.rs1ID, alice_DHPart1FromBob_message->rs1ID,8) != 0) {
			printf ("Alice found that requested shared secret rs1 ID differs!\n");
		} else {
			printf("Alice validate rs1ID from bob DHPart1\n");
		}
	}
	if (contextAlice->cachedSecret.rs2!=NULL) {
		if (memcmp(contextAlice->responderCachedSecretHash.rs2ID, alice_DHPart1FromBob_message->rs2ID,8) != 0) {
			printf ("Alice found that requested shared secret rs2 ID differs!\n");
		} else {
			printf("Alice validate rs2ID from bob DHPart1\n");
		}
	}
	if (contextAlice->cachedSecret.auxsecret!=NULL) {
		if (memcmp(contextAlice->responderCachedSecretHash.auxsecretID, alice_DHPart1FromBob_message->auxsecretID,8) != 0) {
			printf ("Alice found that requested shared secret aux secret ID differs!\n");
		} else {
			printf("Alice validate aux secret ID from bob DHPart1\n");
		}
	}
	if (contextAlice->cachedSecret.pbxsecret!=NULL) {
		if (memcmp(contextAlice->responderCachedSecretHash.pbxsecretID, alice_DHPart1FromBob_message->pbxsecretID,8) != 0) {
			printf ("Alice found that requested shared secret pbx secret ID differs!\n");
		} else {
			printf("Alice validate pbxsecretID from bob DHPart1\n");
		}
	}

	/* Now Alice shall check that the PV from bob is not 1 or Prime-1 TODO*/
	/* Compute the shared DH secret */
	contextAlice->DHMContext->peer = alice_DHPart1FromBob_message->pv;
	bzrtpCrypto_DHMComputeSecret(contextAlice->DHMContext, (int (*)(void *, uint8_t *, uint16_t))bzrtpCrypto_getRandom, (void *)contextAlice->RNGContext);

	/* So Alice send bob her DHPart2 message which is already prepared and stored (we just need to update the sequence number) */
	bzrtp_packetUpdateSequenceNumber(contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID], contextAlice->selfSequenceNumber);
	contextAlice->selfSequenceNumber++;

	/* Bob parse Alice's DHPart2 message */
	bzrtpPacket_t *bob_DHPart2FromAlice = bzrtp_packetCheck(contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID]->packetString, contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength+16, contextBob->peerSequenceNumber, &retval);
	printf ("Bob checking DHPart2 returns %x\n", retval);
	retval += bzrtp_packetParser(contextBob, contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID]->packetString, contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength+16, bob_DHPart2FromAlice);
	printf ("Bob parsing DHPart2 returns %x\n", retval);
	bzrtpDHPartMessage_t *bob_DHPart2FromAlice_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextBob->peerSequenceNumber = bob_DHPart2FromAlice->sequenceNumber;
		bob_DHPart2FromAlice_message = (bzrtpDHPartMessage_t *)bob_DHPart2FromAlice->messageData;
		memcpy(contextBob->peerH[1], bob_DHPart2FromAlice_message->H1, 32);
		contextBob->peerPackets[DHPART2_MESSAGE_STORE_ID] = bob_DHPart2FromAlice;
	}
	packetDump(bob_DHPart2FromAlice, 0);

	/* Now Bob may check which shared secret she expected and if they are valid in bob's DHPart1 */
	if (contextBob->cachedSecret.rs1!=NULL) {
		if (memcmp(contextBob->initiatorCachedSecretHash.rs1ID, bob_DHPart2FromAlice_message->rs1ID,8) != 0) {
			printf ("Bob found that requested shared secret rs1 ID differs!\n");
		} else {
			printf("Bob validate rs1ID from Alice DHPart2\n");
		}
	}
	if (contextBob->cachedSecret.rs2!=NULL) {
		if (memcmp(contextBob->initiatorCachedSecretHash.rs2ID, bob_DHPart2FromAlice_message->rs2ID,8) != 0) {
			printf ("Bob found that requested shared secret rs2 ID differs!\n");
		} else {
			printf("Bob validate rs2ID from Alice DHPart2\n");
		}
	}
	if (contextBob->cachedSecret.auxsecret!=NULL) {
		if (memcmp(contextBob->initiatorCachedSecretHash.auxsecretID, bob_DHPart2FromAlice_message->auxsecretID,8) != 0) {
			printf ("Bob found that requested shared secret aux secret ID differs!\n");
		} else {
			printf("Bob validate aux secret ID from Alice DHPart2\n");
		}
	}
	if (contextBob->cachedSecret.pbxsecret!=NULL) {
		if (memcmp(contextBob->initiatorCachedSecretHash.pbxsecretID, bob_DHPart2FromAlice_message->pbxsecretID,8) != 0) {
			printf ("Bob found that requested shared secret pbx secret ID differs!\n");
		} else {
			printf("Bob validate pbxsecretID from Alice DHPart2\n");
		}
	}

	/* Now Bob shall check that the PV from Alice is not 1 or Prime-1 TODO*/
	/* Compute the shared DH secret */
	contextBob->DHMContext->peer = bob_DHPart2FromAlice_message->pv;
	bzrtpCrypto_DHMComputeSecret(contextBob->DHMContext, (int (*)(void *, uint8_t *, uint16_t))bzrtpCrypto_getRandom, (void *)contextAlice->RNGContext);


	/* JUST FOR TEST: check that the generated secrets are the same */
	uint16_t secretLength = bob_DHPart2FromAlice->messageLength-84; /* length of generated secret is the same than public value */
	if (memcmp(contextBob->DHMContext->key, contextAlice->DHMContext->key, secretLength)==0) {
		printf("Secret Key correctly exchanged \n");
		CU_PASS("Secret Key exchange OK");
	} else {
		CU_FAIL("Secret Key exchange failed");
		printf("ERROR : secretKey exchange failed!!\n");
	}

	/* now compute the total_hash as in rfc section 4.4.1.4 
	 * total_hash = hash(Hello of responder || Commit || DHPart1 || DHPart2)
	 */
	uint16_t totalHashDataLength = bob_Hello->messageLength + alice_Commit->messageLength + contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength + alice_selfDHPart->messageLength;
	uint8_t *dataToHash = (uint8_t *)malloc(totalHashDataLength*sizeof(uint8_t));
	uint16_t hashDataIndex = 0;
	/* get all data from Alice */
	memcpy(dataToHash, contextAlice->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextAlice->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextAlice->selfPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextAlice->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
	if (contextAlice->peerPackets[DHPART2_MESSAGE_STORE_ID]->packetString == NULL) {printf("NULL le truc\n");}
	memcpy(dataToHash+hashDataIndex, contextAlice->peerPackets[DHPART2_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->peerPackets[DHPART2_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextAlice->peerPackets[DHPART2_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength);

	uint8_t alice_totalHash[32]; /* Note : actual length of hash depends on the choosen algo */
	contextAlice->hashFunction(dataToHash, totalHashDataLength, 32, alice_totalHash);

	/* get all data from Bob */
	hashDataIndex = 0;
	memcpy(dataToHash, contextBob->selfPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextBob->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextBob->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextBob->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextBob->selfPackets[DHPART2_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextBob->peerPackets[DHPART2_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->peerPackets[DHPART2_MESSAGE_STORE_ID]->messageLength);

	uint8_t bob_totalHash[32]; /* Note : actual length of hash depends on the choosen algo */
	contextBob->hashFunction(dataToHash, totalHashDataLength, 32, bob_totalHash);
	if (memcmp(bob_totalHash, alice_totalHash, 32) == 0) {
		printf("Got the same total hash\n");
		CU_PASS("Total Hash match");
	} else {
		printf("AARGG!! total hash mismatch");
		CU_FAIL("Total Hash mismatch");
	}

	/* now compute s0 and KDF_context as in rfc section 4.4.1.4 
		s0 = hash(counter || DHResult || "ZRTP-HMAC-KDF" || ZIDi || ZIDr || total_hash || len(s1) || s1 || len(s2) || s2 || len(s3) || s3)
		counter is a fixed 32 bits integer in big endian set to 1 : 0x00000001
	*/
	free(dataToHash);
	contextAlice->KDFContextLength = 24+32;/* actual depends on selected hash length*/
	contextAlice->KDFContext = (uint8_t *)malloc(contextAlice->KDFContextLength*sizeof(uint8_t));
	memcpy(contextAlice->KDFContext, contextAlice->selfZID, 12); /* ZIDi*/
	memcpy(contextAlice->KDFContext+12, contextAlice->peerZID, 12); /* ZIDr */
	memcpy(contextAlice->KDFContext+24, alice_totalHash, 32); /* total Hash*/

	uint8_t *s1=NULL;
	uint32_t s1Length=0;
	uint8_t *s2=NULL;
	uint32_t s2Length=0;
	uint8_t *s3=NULL;
	uint32_t s3Length=0;
	/* get s1 from rs1 or rs2 */
	if (contextAlice->cachedSecret.rs1 != NULL) { /* if there is a s1 (already validated when received the DHpacket) */
		s1 = contextAlice->cachedSecret.rs1;
		s1Length = contextAlice->cachedSecret.rs1Length;
	} else if (contextAlice->cachedSecret.rs2 != NULL) { /* otherwise if there is a s2 (already validated when received the DHpacket) */
		s1 = contextAlice->cachedSecret.rs2;
		s1Length = contextAlice->cachedSecret.rs2Length;
	}

	/* s2 is the auxsecret */
	s2 = contextAlice->cachedSecret.auxsecret; /* this may be null if no match or no aux secret where found */
	s2Length = contextAlice->cachedSecret.auxsecretLength; /* this may be 0 if no match or no aux secret where found */

	/* s3 is the pbxsecret */
	s3 = contextAlice->cachedSecret.pbxsecret; /* this may be null if no match or no pbx secret where found */
	s3Length = contextAlice->cachedSecret.pbxsecretLength; /* this may be 0 if no match or no pbx secret where found */
	
	totalHashDataLength = 4+secretLength+13/*ZRTP-HMAC-KDF string*/ + 12 + 12 + 32 + 4 +s1Length + 4 +s2Length + 4 + s3Length; /* secret length was computed before as the length of DH secret data */

	dataToHash = (uint8_t *)malloc(totalHashDataLength*sizeof(uint8_t));
	dataToHash[0] = 0x00;
	dataToHash[1] = 0x00;
	dataToHash[2] = 0x00;
	dataToHash[3] = 0x01;
	hashDataIndex = 4;
	
	memcpy(dataToHash+hashDataIndex, contextAlice->DHMContext->key, secretLength);
	hashDataIndex += secretLength;
	memcpy(dataToHash+hashDataIndex, "ZRTP-HMAC-KDF", 13);
	hashDataIndex += 13;
	memcpy(dataToHash+hashDataIndex, contextAlice->KDFContext, contextAlice->KDFContextLength);
	hashDataIndex += 56;

	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s1Length&0xFF);
	if (s1!=NULL) {
		memcpy(dataToHash+hashDataIndex, s1, s1Length);
		hashDataIndex += s1Length;
	}

	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s2Length&0xFF);
	if (s2!=NULL) {
		memcpy(dataToHash+hashDataIndex, s2, s2Length);
		hashDataIndex += s2Length;
	}

	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s3Length&0xFF);
	if (s3!=NULL) {
		memcpy(dataToHash+hashDataIndex, s3, s3Length);
		hashDataIndex += s3Length;
	}

	contextAlice->s0 = (uint8_t *)malloc(32*sizeof(uint8_t));
	contextAlice->hashFunction(dataToHash, totalHashDataLength, 32, contextAlice->s0);

	/* destroy all cached keys in context */
	if (contextAlice->cachedSecret.rs1!=NULL) {
		bzrtp_DestroyKey(contextAlice->cachedSecret.rs1, contextAlice->cachedSecret.rs1Length, contextAlice->RNGContext);
		free(contextAlice->cachedSecret.rs1);
		contextAlice->cachedSecret.rs1 = NULL;
	}
	if (contextAlice->cachedSecret.rs2!=NULL) {
		bzrtp_DestroyKey(contextAlice->cachedSecret.rs2, contextAlice->cachedSecret.rs2Length, contextAlice->RNGContext);
		free(contextAlice->cachedSecret.rs2);
		contextAlice->cachedSecret.rs2 = NULL;
	}
	if (contextAlice->cachedSecret.auxsecret!=NULL) {
		bzrtp_DestroyKey(contextAlice->cachedSecret.auxsecret, contextAlice->cachedSecret.auxsecretLength, contextAlice->RNGContext);
		free(contextAlice->cachedSecret.auxsecret);
		contextAlice->cachedSecret.auxsecret = NULL;
	}
	if (contextAlice->cachedSecret.pbxsecret!=NULL) {
		bzrtp_DestroyKey(contextAlice->cachedSecret.pbxsecret, contextAlice->cachedSecret.pbxsecretLength, contextAlice->RNGContext);
		free(contextAlice->cachedSecret.pbxsecret);
		contextAlice->cachedSecret.pbxsecret = NULL;
	}

	/*** Do the same for bob ***/
	/* get s1 from rs1 or rs2 */
	s1=NULL;
	s2=NULL;
	s3=NULL;
	contextBob->KDFContextLength = 24+32;/* actual depends on selected hash length*/
	contextBob->KDFContext = (uint8_t *)malloc(contextBob->KDFContextLength*sizeof(uint8_t));
	memcpy(contextBob->KDFContext, contextBob->peerZID, 12); /* ZIDi*/
	memcpy(contextBob->KDFContext+12, contextBob->selfZID, 12); /* ZIDr */
	memcpy(contextBob->KDFContext+24, bob_totalHash, 32); /* total Hash*/

	if (contextBob->cachedSecret.rs1 != NULL) { /* if there is a s1 (already validated when received the DHpacket) */
		s1 = contextBob->cachedSecret.rs1;
		s1Length = contextBob->cachedSecret.rs1Length;
	} else if (contextBob->cachedSecret.rs2 != NULL) { /* otherwise if there is a s2 (already validated when received the DHpacket) */
		s1 = contextBob->cachedSecret.rs2;
		s1Length = contextBob->cachedSecret.rs2Length;
	}

	/* s2 is the auxsecret */
	s2 = contextBob->cachedSecret.auxsecret; /* this may be null if no match or no aux secret where found */
	s2Length = contextBob->cachedSecret.auxsecretLength; /* this may be 0 if no match or no aux secret where found */

	/* s3 is the pbxsecret */
	s3 = contextBob->cachedSecret.pbxsecret; /* this may be null if no match or no pbx secret where found */
	s3Length = contextBob->cachedSecret.pbxsecretLength; /* this may be 0 if no match or no pbx secret where found */

	free(dataToHash);
	dataToHash = (uint8_t *)malloc(totalHashDataLength*sizeof(uint8_t));
	dataToHash[0] = 0x00;
	dataToHash[1] = 0x00;
	dataToHash[2] = 0x00;
	dataToHash[3] = 0x01;
	hashDataIndex = 4;
	
	memcpy(dataToHash+hashDataIndex, contextBob->DHMContext->key, secretLength);
	hashDataIndex += secretLength;
	memcpy(dataToHash+hashDataIndex, "ZRTP-HMAC-KDF", 13);
	hashDataIndex += 13;
	memcpy(dataToHash+hashDataIndex, contextBob->KDFContext, contextBob->KDFContextLength);
	hashDataIndex += 56;

	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s1Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s1Length&0xFF);
	if (s1!=NULL) {
		memcpy(dataToHash+hashDataIndex, s1, s1Length);
		hashDataIndex += s1Length;
	}

	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s2Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s2Length&0xFF);
	if (s2!=NULL) {
		memcpy(dataToHash+hashDataIndex, s2, s2Length);
		hashDataIndex += s2Length;
	}

	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>24)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>16)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)((s3Length>>8)&0xFF);
	dataToHash[hashDataIndex++] = (uint8_t)(s3Length&0xFF);
	if (s3!=NULL) {
		memcpy(dataToHash+hashDataIndex, s3, s3Length);
		hashDataIndex += s3Length;
	}

	contextBob->s0 = (uint8_t *)malloc(32*sizeof(uint8_t));
	contextBob->hashFunction(dataToHash, totalHashDataLength, 32, contextBob->s0);
	

	/* destroy all cached keys in context */
	if (contextBob->cachedSecret.rs1!=NULL) {
		bzrtp_DestroyKey(contextBob->cachedSecret.rs1, contextBob->cachedSecret.rs1Length, contextBob->RNGContext);
		free(contextBob->cachedSecret.rs1);
		contextBob->cachedSecret.rs1 = NULL;
	}
	if (contextBob->cachedSecret.rs2!=NULL) {
		bzrtp_DestroyKey(contextBob->cachedSecret.rs2, contextBob->cachedSecret.rs2Length, contextBob->RNGContext);
		free(contextBob->cachedSecret.rs2);
		contextBob->cachedSecret.rs2 = NULL;
	}
	if (contextBob->cachedSecret.auxsecret!=NULL) {
		bzrtp_DestroyKey(contextBob->cachedSecret.auxsecret, contextBob->cachedSecret.auxsecretLength, contextBob->RNGContext);
		free(contextBob->cachedSecret.auxsecret);
		contextBob->cachedSecret.auxsecret = NULL;
	}
	if (contextBob->cachedSecret.pbxsecret!=NULL) {
		bzrtp_DestroyKey(contextBob->cachedSecret.pbxsecret, contextBob->cachedSecret.pbxsecretLength, contextBob->RNGContext);
		free(contextBob->cachedSecret.pbxsecret);
		contextBob->cachedSecret.pbxsecret = NULL;
	}


	/* DEBUG compare s0 */
	if (memcmp(contextBob->s0, contextAlice->s0, 32)==0) {
		printf("Got the same s0\n");
		CU_PASS("s0 match");
	} else {
		printf("ERROR s0 differs\n");
		CU_PASS("s0 mismatch");
	}

	/* now compute the ZRTPSession key : section 4.5.2
	 * ZRTPSess = KDF(s0, "ZRTP Session Key", KDF_Context, negotiated hash length)*/
	contextAlice->ZRTPSessLength=32; /* must be set to the length of negotiated hash */
	contextAlice->ZRTPSess = (uint8_t *)malloc(contextAlice->ZRTPSessLength*sizeof(uint8_t));
	retval = bzrtp_keyDerivationFunction(contextAlice->s0, contextAlice->hashLength,
		(uint8_t *)"ZRTP Session Key", 16,
		contextAlice->KDFContext, contextAlice->KDFContextLength, /* this one too depends on selected hash */
		32,
		(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->hmacFunction,
		contextAlice->ZRTPSess);

	contextBob->ZRTPSessLength=32; /* must be set to the length of negotiated hash */
	contextBob->ZRTPSess = (uint8_t *)malloc(contextBob->ZRTPSessLength*sizeof(uint8_t));
	retval = bzrtp_keyDerivationFunction(contextBob->s0, contextBob->hashLength,
		(uint8_t *)"ZRTP Session Key", 16,
		contextBob->KDFContext, contextBob->KDFContextLength, /* this one too depends on selected hash */
		32,
		(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->hmacFunction,
		contextBob->ZRTPSess);

	/* DEBUG compare ZRTPSess Key */
	if (memcmp(contextBob->ZRTPSess, contextAlice->ZRTPSess, 32)==0) {
		printf("Got the same ZRTPSess\n");
		CU_PASS("ZRTPSess match");
	} else {
		printf("ERROR ZRTPSess differs\n");
		CU_PASS("ZRTPSess mismatch");
	}


	/* compute the sas according to rfc section 4.5.2 sashash = KDF(s0, "SAS", KDF_Context, 256) */
	uint8_t alice_sasHash[32];
	retval = bzrtp_keyDerivationFunction(contextAlice->s0, contextAlice->hashLength,
			(uint8_t *)"SAS", 3,
			contextAlice->KDFContext, contextAlice->KDFContextLength, /* this one too depends on selected hash */
			256/8, /* function gets L in bytes */
			(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->hmacFunction,
			alice_sasHash);

	uint8_t bob_sasHash[32];
	retval = bzrtp_keyDerivationFunction(contextBob->s0, contextBob->hashLength,
			(uint8_t *)"SAS", 3,
			contextBob->KDFContext, contextBob->KDFContextLength, /* this one too depends on selected hash */
			256/8, /* function gets L in bytes */
			(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->hmacFunction,
			bob_sasHash);

	/* DEBUG compare sasHash */
	if (memcmp(alice_sasHash, bob_sasHash, 32)==0) {
		printf("Got the same SAS Hash\n");
		CU_PASS("SAS Hash match");
	} else {
		printf("ERROR SAS Hash differs\n");
		CU_PASS("SAS Hash mismatch");
	}

	/* display SAS (we shall not do this now but after the confirm message exchanges) */
	uint32_t sasValue = ((uint32_t)alice_sasHash[0]<<24) | ((uint32_t)alice_sasHash[1]<<16) | ((uint32_t)alice_sasHash[2]<<8) | ((uint32_t)(alice_sasHash[3]));
	char sas[4];
	contextAlice->sasFunction(sasValue, sas);

	printf("Alice SAS is %.4s\n", sas);

	sasValue = ((uint32_t)bob_sasHash[0]<<24) | ((uint32_t)bob_sasHash[1]<<16) | ((uint32_t)bob_sasHash[2]<<8) | ((uint32_t)(bob_sasHash[3]));
	contextBob->sasFunction(sasValue, sas);
	
	printf("Bob SAS is %.4s\n", sas);


	/* now derive the other keys (mackeyi, mackeyr, zrtpkeyi and zrtpkeyr, srtpkeys and salt) */
	contextAlice->mackeyi = (uint8_t *)malloc(contextAlice->hashLength*(sizeof(uint8_t)));
	contextAlice->mackeyr = (uint8_t *)malloc(contextAlice->hashLength*(sizeof(uint8_t)));
	contextAlice->zrtpkeyi = (uint8_t *)malloc(contextAlice->cipherKeyLength*(sizeof(uint8_t)));
	contextAlice->zrtpkeyr = (uint8_t *)malloc(contextAlice->cipherKeyLength*(sizeof(uint8_t)));
	contextBob->mackeyi = (uint8_t *)malloc(contextBob->hashLength*(sizeof(uint8_t)));
	contextBob->mackeyr = (uint8_t *)malloc(contextBob->hashLength*(sizeof(uint8_t)));
	contextBob->zrtpkeyi = (uint8_t *)malloc(contextBob->cipherKeyLength*(sizeof(uint8_t)));
	contextBob->zrtpkeyr = (uint8_t *)malloc(contextBob->cipherKeyLength*(sizeof(uint8_t)));

	/* Alice */
	retval = bzrtp_keyDerivationFunction(contextAlice->s0, contextAlice->hashLength, (uint8_t *)"Initiator HMAC key", 18, contextAlice->KDFContext, contextAlice->KDFContextLength, contextAlice->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->hmacFunction, contextAlice->mackeyi);
	retval += bzrtp_keyDerivationFunction(contextAlice->s0, contextAlice->hashLength, (uint8_t *)"Responder HMAC key", 18, contextAlice->KDFContext, contextAlice->KDFContextLength, contextAlice->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->hmacFunction, contextAlice->mackeyr);
	retval += bzrtp_keyDerivationFunction(contextAlice->s0, contextAlice->hashLength, (uint8_t *)"Initiator ZRTP key", 18, contextAlice->KDFContext, contextAlice->KDFContextLength, contextAlice->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->hmacFunction, contextAlice->zrtpkeyi);
	retval += bzrtp_keyDerivationFunction(contextAlice->s0, contextAlice->hashLength, (uint8_t *)"Responder ZRTP key", 18, contextAlice->KDFContext, contextAlice->KDFContextLength, contextAlice->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->hmacFunction, contextAlice->zrtpkeyr);

	/* Bob */
	retval = bzrtp_keyDerivationFunction(contextBob->s0, contextBob->hashLength, (uint8_t *)"Initiator HMAC key", 18, contextBob->KDFContext, contextBob->KDFContextLength, contextBob->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->hmacFunction, contextBob->mackeyi);
	retval += bzrtp_keyDerivationFunction(contextBob->s0, contextBob->hashLength, (uint8_t *)"Responder HMAC key", 18, contextBob->KDFContext, contextBob->KDFContextLength, contextBob->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->hmacFunction, contextBob->mackeyr);
	retval += bzrtp_keyDerivationFunction(contextBob->s0, contextBob->hashLength, (uint8_t *)"Initiator ZRTP key", 18, contextBob->KDFContext, contextBob->KDFContextLength, contextBob->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->hmacFunction, contextBob->zrtpkeyi);
	retval += bzrtp_keyDerivationFunction(contextBob->s0, contextBob->hashLength, (uint8_t *)"Responder ZRTP key", 18, contextBob->KDFContext, contextBob->KDFContextLength, contextBob->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->hmacFunction, contextBob->zrtpkeyr);


	/* DEBUG compare keys */
	if ((memcmp(contextAlice->mackeyi, contextBob->mackeyi, contextAlice->hashLength)==0) && (memcmp(contextAlice->mackeyr, contextBob->mackeyr, contextAlice->hashLength)==0) && (memcmp(contextAlice->zrtpkeyi, contextBob->zrtpkeyi, contextAlice->cipherKeyLength)==0) && (memcmp(contextAlice->zrtpkeyr, contextBob->zrtpkeyr, contextAlice->cipherKeyLength)==0)) {
		printf("Got the same keys\n");
		CU_PASS("keys match");
	} else {
		printf("ERROR keys differ\n");
		CU_PASS("Keys mismatch");
	}

	/* now Bob build the CONFIRM1 packet and send it to Alice */
	bzrtpPacket_t *bob_Confirm1 = bzrtp_createZrtpPacket(contextBob, MSGTYPE_CONFIRM1, 0x87654321, &retval);
	retval += bzrtp_packetBuild(contextBob, bob_Confirm1, contextBob->selfSequenceNumber);
	if (retval == 0) {
		contextBob->selfSequenceNumber++;
	}
	printf("Bob building Commit return %x\n", retval);

	bzrtpPacket_t *alice_Confirm1FromBob = bzrtp_packetCheck(bob_Confirm1->packetString, bob_Confirm1->messageLength+16, contextAlice->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, bob_Confirm1->packetString, bob_Confirm1->messageLength+16, alice_Confirm1FromBob);
	printf ("Alice parsing confirm1 returns %x\n", retval);
	bzrtpConfirmMessage_t *alice_Confirm1FromBob_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->peerSequenceNumber = alice_Confirm1FromBob->sequenceNumber;
		alice_Confirm1FromBob_message = (bzrtpConfirmMessage_t *)alice_Confirm1FromBob->messageData;
		memcpy(contextAlice->peerH[0], alice_Confirm1FromBob_message->H0, 32);
	}

	packetDump(bob_Confirm1,1);
	packetDump(alice_Confirm1FromBob,0);

	/* So now alice got all the H[0-3] from bob, so check the messages using this MAC : DHPart1[H0] - Hello[H2] */
	/* And check also the hashChain - NOTE: this shall be done upon reception of each H[0-3] value in a packet and packet rejected if failed */

	/* now Alice build the CONFIRM2 packet and send it to Bob */
	bzrtpPacket_t *alice_Confirm2 = bzrtp_createZrtpPacket(contextAlice, MSGTYPE_CONFIRM2, 0x12345678, &retval);
	retval += bzrtp_packetBuild(contextAlice, alice_Confirm2, contextAlice->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->selfSequenceNumber++;
	}
	printf("Alice building Confirm2 return %x\n", retval);

	bzrtpPacket_t *bob_Confirm2FromAlice = bzrtp_packetCheck(alice_Confirm2->packetString, alice_Confirm2->messageLength+16, contextBob->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextBob, alice_Confirm2->packetString, alice_Confirm2->messageLength+16, bob_Confirm2FromAlice);
	printf ("Bob parsing confirm2 returns %x\n", retval);
	bzrtpConfirmMessage_t *bob_Confirm2FromAlice_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextBob->peerSequenceNumber = bob_Confirm2FromAlice->sequenceNumber;
		bob_Confirm2FromAlice_message = (bzrtpConfirmMessage_t *)bob_Confirm2FromAlice->messageData;
		memcpy(contextBob->peerH[0], bob_Confirm2FromAlice_message->H0, 32);
	}

	packetDump(alice_Confirm2,1);
	packetDump(bob_Confirm2FromAlice,0);

	/* So now bob got all the H[0-3] from bob, so check the messages using this MAC : DHPart1[H0] - Hello[H2] */
	/* And check also the hashChain - NOTE: this shall be done upon reception of each H[0-3] value in a packet and packet rejected if failed */

	/* Bob build the conf2Ack and send it to Alice */
	bzrtpPacket_t *bob_Conf2ACK =  bzrtp_createZrtpPacket(contextBob, MSGTYPE_CONF2ACK, 0x87654321, &retval);
	retval += bzrtp_packetBuild(contextBob, bob_Conf2ACK, contextBob->selfSequenceNumber);
	if (retval == 0) {
		contextBob->selfSequenceNumber++;
	}
	printf("Bob building Conf2ACK return %x\n", retval);

	bzrtpPacket_t *alice_Conf2ACKFromBob = bzrtp_packetCheck(bob_Conf2ACK->packetString, bob_Conf2ACK->messageLength+16, contextAlice->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, bob_Conf2ACK->packetString, bob_Conf2ACK->messageLength+16, alice_Conf2ACKFromBob);
	printf ("Alice parsing conf2ACK returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		contextBob->peerSequenceNumber = bob_Confirm2FromAlice->sequenceNumber;
	}



	dumpContext("\nAlice", contextAlice);
	dumpContext("\nBob", contextBob);

	/* destroy the context */
	bzrtp_destroyBzrtpContext(contextBob);
	bzrtp_destroyBzrtpContext(contextAlice);

}
