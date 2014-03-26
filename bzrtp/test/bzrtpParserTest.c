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
#include <unistd.h>
#include <sys/time.h>
/* this defines is need to get nanosleep function  from time.h */
#define __USE_POSIX199309
#include <time.h>

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
#define TEST_PACKET_NUMBER 9
/* meta data: length, sequence number, SSRC */
static uint32_t patternZRTPMetaData[TEST_PACKET_NUMBER][3] = {
	{136, 0x09f1, 0x12345678}, /* hello */
	{136, 0x02ce, 0x87654321}, /* hello */
	{28, 0x09f2, 0x12345678}, /* hello ack */
	{132, 0x02d0, 0x87654321}, /* commit */
	{484, 0x09f5, 0x12345678}, /* dhpart1 */
	{484, 0x02d1, 0x87654321}, /* dhpart2 */
	{92, 0x09f6, 0x12345678}, /* confirm 1 */
	{92, 0x02d2, 0x87654321}, /* confirm 2 */
	{28, 0x09f7, 0x12345678} /* conf2ACK*/
};

static const uint8_t patternZRTPPackets[TEST_PACKET_NUMBER][512] = {
	/* This is a Hello packet, sequence number is 0x09f1, SSRC 0x12345678 */
	{0x10, 0x00, 0x09, 0xf1, 0x5a, 0x52, 0x54, 0x50, 0x12, 0x34, 0x56, 0x78, 0x50, 0x5a, 0x00, 0x1e, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x20, 0x20, 0x31, 0x2e, 0x31, 0x30, 0x4c, 0x49, 0x4e, 0x50, 0x48, 0x4f, 0x4e, 0x45, 0x2d, 0x5a, 0x52, 0x54, 0x50, 0x43, 0x50, 0x50, 0xe8, 0xd5, 0x26, 0xc1, 0x3a, 0x0c, 0x4c, 0x6a, 0xce, 0x18, 0xaa, 0xc7, 0xc4, 0xa4, 0x07, 0x0e, 0x65, 0x7a, 0x4d, 0xca, 0x78, 0xf2, 0xcc, 0xcd, 0x20, 0x50, 0x38, 0x73, 0xe9, 0x7e, 0x08, 0x29, 0x7e, 0xb0, 0x04, 0x97, 0xc0, 0xfe, 0xb2, 0xc9, 0x24, 0x31, 0x49, 0x7f, 0x00, 0x01, 0x12, 0x31, 0x53, 0x32, 0x35, 0x36, 0x41, 0x45, 0x53, 0x31, 0x48, 0x53, 0x33, 0x32, 0x48, 0x53, 0x38, 0x30, 0x44, 0x48, 0x33, 0x6b, 0x44, 0x48, 0x32, 0x6b, 0x4d, 0x75, 0x6c, 0x74, 0x42, 0x33, 0x32, 0x20, 0xa0, 0xfd, 0x0f, 0xad, 0xeb, 0xe0, 0x86, 0x56, 0xe3, 0x65, 0x81, 0x02},
	/* This is a Hello packet, sequence number is 0x02ce, SSRC 0x87654321 */
	{0x10, 0x00, 0x02, 0xce, 0x5a, 0x52, 0x54, 0x50, 0x87, 0x65, 0x43, 0x21, 0x50, 0x5a, 0x00, 0x1e, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x20, 0x20, 0x31, 0x2e, 0x31, 0x30, 0x4c, 0x49, 0x4e, 0x50, 0x48, 0x4f, 0x4e, 0x45, 0x2d, 0x5a, 0x52, 0x54, 0x50, 0x43, 0x50, 0x50, 0x8d, 0x0f, 0x5a, 0x20, 0x79, 0x97, 0x42, 0x01, 0x99, 0x45, 0x45, 0xf7, 0x0e, 0x31, 0x06, 0xe1, 0x05, 0xc0, 0xb9, 0x24, 0xe9, 0xc9, 0x78, 0xc7, 0x38, 0xf5, 0x97, 0x48, 0xef, 0x42, 0x6a, 0x3e, 0x92, 0x42, 0xc2, 0xcf, 0x44, 0xee, 0x9c, 0x65, 0xca, 0x58, 0x78, 0xf1, 0x00, 0x01, 0x12, 0x31, 0x53, 0x32, 0x35, 0x36, 0x41, 0x45, 0x53, 0x31, 0x48, 0x53, 0x33, 0x32, 0x48, 0x53, 0x38, 0x30, 0x44, 0x48, 0x33, 0x6b, 0x44, 0x48, 0x32, 0x6b, 0x4d, 0x75, 0x6c, 0x74, 0x42, 0x33, 0x32, 0x20, 0xb3, 0x90, 0x91, 0x95, 0xe4, 0x67, 0xa3, 0x21, 0xe3, 0x5f, 0x9c, 0x92},
	/* This is a Hello ack packet, sequence number is 0x09f2, SSRC 0x12345678*/
	{0x10, 0x00, 0x09, 0xf2, 0x5a, 0x52, 0x54, 0x50, 0x12, 0x34, 0x56, 0x78, 0x50, 0x5a, 0x00, 0x03, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x41, 0x43, 0x4b, 0x77, 0x0e, 0x44, 0x07},
	/* This is a Commit packet, sequence number is 0x02d0, SSRC 0x87654321 */
	{0x10, 0x00, 0x02, 0xd0, 0x5a, 0x52, 0x54, 0x50, 0x87, 0x65, 0x43, 0x21, 0x50, 0x5a, 0x00, 0x1d, 0x43, 0x6f, 0x6d, 0x6d, 0x69, 0x74, 0x20, 0x20, 0xd9, 0xff, 0x14, 0x8b, 0x34, 0xaa, 0x69, 0xe9, 0x33, 0xc1, 0x62, 0xe6, 0x6b, 0xe8, 0xcd, 0x9d, 0xe3, 0x0f, 0xb7, 0x6a, 0xe8, 0x6a, 0x62, 0x2b, 0xcb, 0xe4, 0x6b, 0x91, 0x05, 0xc7, 0xc8, 0x7e, 0x92, 0x42, 0xc2, 0xcf, 0x44, 0xee, 0x9c, 0x65, 0xca, 0x58, 0x78, 0xf1, 0x53, 0x32, 0x35, 0x36, 0x41, 0x45, 0x53, 0x31, 0x48, 0x53, 0x33, 0x32, 0x44, 0x48, 0x33, 0x6b, 0x42, 0x33, 0x32, 0x20, 0x1e, 0xc0, 0xfe, 0x2e, 0x72, 0x06, 0x4d, 0xfb, 0xfc, 0x92, 0x02, 0x8c, 0x03, 0x0f, 0xb8, 0xf8, 0x91, 0xb4, 0xe7, 0x96, 0xac, 0x25, 0xfd, 0xf9, 0x68, 0xc6, 0xe9, 0x67, 0xa9, 0x42, 0xb1, 0x5b, 0xbb, 0x6d, 0x9c, 0xd2, 0x4b, 0x13, 0xa9, 0xae, 0x25, 0x5c, 0xa9, 0xc1},
	/* This is a DHPart1 packet, sequence number is 0x09f5, SSRC 0x12345678 */
	{0x10, 0x00, 0x09, 0xf5, 0x5a, 0x52, 0x54, 0x50, 0x12, 0x34, 0x56, 0x78, 0x50, 0x5a, 0x00, 0x75, 0x44, 0x48, 0x50, 0x61, 0x72, 0x74, 0x31, 0x20, 0x28, 0x7c, 0x28, 0xe4, 0xd7, 0x3d, 0x14, 0x39, 0xb5, 0x6d, 0x1c, 0x47, 0x9d, 0x59, 0x0a, 0xf2, 0x10, 0x33, 0xde, 0x6b, 0xd5, 0x2b, 0xfb, 0x26, 0xa5, 0x87, 0x4d, 0xe9, 0x20, 0x6b, 0x9f, 0xdd, 0xab, 0xbc, 0xc6, 0x8d, 0xbd, 0x5d, 0xe6, 0x67, 0x00, 0x69, 0x44, 0xb1, 0x84, 0x2c, 0x27, 0x10, 0x8c, 0x4e, 0x58, 0x8a, 0xed, 0x7e, 0x8b, 0x44, 0x2c, 0x3a, 0x13, 0x02, 0xdf, 0x58, 0xb6, 0xda, 0x80, 0x55, 0xec, 0xb0, 0x20, 0xc7, 0x76, 0x50, 0xc4, 0x1b, 0xa8, 0x26, 0x11, 0x5c, 0xf5, 0x71, 0x7e, 0xb4, 0x86, 0x22, 0x17, 0xde, 0x14, 0x08, 0x46, 0x5c, 0xac, 0x88, 0x8c, 0x41, 0x6b, 0x95, 0x22, 0xba, 0xf8, 0x3e, 0x67, 0x20, 0x94, 0xa0, 0x84, 0xa3, 0x93, 0x41, 0x9a, 0x1a, 0x7c, 0x2f, 0x04, 0xf4, 0x14, 0x3f, 0x11, 0x54, 0x02, 0xba, 0xee, 0xc2, 0x20, 0xfa, 0x38, 0xf7, 0xba, 0xa4, 0xbf, 0x4a, 0x70, 0x02, 0x68, 0xdc, 0xb2, 0xe9, 0x1a, 0x8a, 0x87, 0xa5, 0xe4, 0x9c, 0x42, 0x07, 0x82, 0x26, 0xb4, 0xda, 0xe3, 0x1b, 0xdc, 0x78, 0xc7, 0xd8, 0xa8, 0x00, 0x5c, 0x00, 0x14, 0xe4, 0x00, 0xfe, 0x6c, 0x2d, 0xce, 0x62, 0xc9, 0x71, 0x5d, 0xed, 0x4e, 0x66, 0x9f, 0xf5, 0x30, 0xc0, 0x04, 0x53, 0xf6, 0x15, 0x2f, 0xe1, 0x85, 0x3b, 0xd9, 0x40, 0x9b, 0x50, 0x07, 0x43, 0x7c, 0x36, 0x01, 0xa1, 0xda, 0x66, 0xc4, 0x20, 0x2f, 0x45, 0xc0, 0xcc, 0xb2, 0x64, 0x63, 0x9c, 0x07, 0x9d, 0x23, 0x27, 0x80, 0xa1, 0x7f, 0xc2, 0xe0, 0xa0, 0xfd, 0xc3, 0x98, 0x83, 0xa3, 0xaa, 0x6b, 0xdc, 0x9f, 0x6a, 0xc3, 0x32, 0x94, 0xf0, 0x80, 0xa0, 0xd9, 0xf1, 0x83, 0x41, 0x48, 0xa9, 0xb5, 0xed, 0x62, 0x50, 0x88, 0xec, 0x33, 0x32, 0xd2, 0x5f, 0xdc, 0xcc, 0xae, 0xc9, 0x74, 0xca, 0x0a, 0xab, 0x82, 0x06, 0x01, 0x46, 0x35, 0x30, 0xcd, 0x68, 0xec, 0x09, 0xab, 0x8c, 0xb0, 0x39, 0xe5, 0xd8, 0x5c, 0xa2, 0xe4, 0x82, 0xfe, 0x46, 0x01, 0xbd, 0xe7, 0x7f, 0x60, 0x1b, 0x50, 0x62, 0xfb, 0x6f, 0xee, 0x6c, 0xf1, 0xf7, 0x9b, 0xb7, 0x1c, 0x76, 0x48, 0xb5, 0xbe, 0xa5, 0x83, 0x73, 0x07, 0xa2, 0xe2, 0x73, 0xc7, 0x68, 0x34, 0xc8, 0xef, 0x12, 0xc4, 0x32, 0xdf, 0x37, 0x3d, 0xdc, 0x07, 0x0e, 0xa6, 0x92, 0x82, 0xbd, 0xba, 0x20, 0xc4, 0xb4, 0x8d, 0x1f, 0x19, 0x1c, 0x15, 0x0f, 0x5f, 0x01, 0xdf, 0x67, 0x1f, 0x59, 0xd1, 0x5e, 0x99, 0x60, 0xf6, 0xb8, 0x67, 0xe2, 0x46, 0x62, 0x11, 0x30, 0xfb, 0x81, 0x9d, 0x0b, 0xec, 0x36, 0xe9, 0x8d, 0x43, 0xfe, 0x55, 0xd9, 0x61, 0x98, 0x3f, 0x2e, 0x39, 0x6a, 0x26, 0x43, 0xb0, 0x6d, 0x08, 0xec, 0x2e, 0x42, 0x7e, 0x23, 0x82, 0x6f, 0xd9, 0xbb, 0xfd, 0x0a, 0xcd, 0x48, 0xe7, 0xd5, 0x8b, 0xa5, 0x80, 0x34, 0xca, 0x96, 0x4f, 0x58, 0x25, 0xba, 0x43, 0x5e, 0x3d, 0x1c, 0xee, 0x72, 0xb8, 0x35, 0x8c, 0x5d, 0xd9, 0x69, 0x24, 0x58, 0x36, 0x21, 0xb0, 0x45, 0xa4, 0xad, 0x40, 0xda, 0x94, 0x98, 0x0f, 0xb1, 0xed, 0x6c, 0xad, 0x26, 0x03, 0x04, 0x82, 0xff, 0x15, 0x00, 0x41, 0x87, 0x06, 0x93, 0xd4, 0x86, 0xa9, 0x7e, 0xb8, 0xd9, 0x70, 0x34, 0x6d, 0x8e, 0x6a, 0x16, 0xe2, 0x46, 0x52, 0xb0, 0x78, 0x54, 0x53, 0xaf},
	/* This is a DHPart2 packet, sequence number is 0x02d1, SSRC 0x87654321 */
	{0x10, 0x00, 0x02, 0xd1, 0x5a, 0x52, 0x54, 0x50, 0x87, 0x65, 0x43, 0x21, 0x50, 0x5a, 0x00, 0x75, 0x44, 0x48, 0x50, 0x61, 0x72, 0x74, 0x32, 0x20, 0x9e, 0xb2, 0xa5, 0x8b, 0xe8, 0x96, 0x37, 0xf5, 0x5a, 0x41, 0x34, 0xb2, 0xec, 0xda, 0x84, 0x95, 0xf0, 0xf8, 0x9b, 0xab, 0x61, 0x4f, 0x7c, 0x9e, 0x56, 0xb7, 0x3b, 0xd3, 0x46, 0xba, 0xbe, 0x9a, 0xae, 0x97, 0x97, 0xda, 0x5f, 0x9f, 0x89, 0xba, 0xfe, 0x61, 0x66, 0x5f, 0x9e, 0xa4, 0x5b, 0xcb, 0xd5, 0x69, 0xcf, 0xfb, 0xfd, 0xdc, 0xac, 0x79, 0xac, 0x1d, 0x0b, 0xe5, 0x15, 0x75, 0x39, 0x2e, 0xe5, 0xa9, 0x2a, 0x60, 0xd7, 0xe3, 0x48, 0xd0, 0x1f, 0xd8, 0x61, 0x65, 0xfd, 0x2e, 0x5c, 0xef, 0x43, 0xf5, 0x63, 0x99, 0xb3, 0x45, 0x25, 0xe3, 0xbc, 0xf0, 0xe1, 0xad, 0xf7, 0x84, 0xcd, 0x82, 0x20, 0xe3, 0x6f, 0x2c, 0x77, 0x51, 0xf1, 0x11, 0x2e, 0x4a, 0x2c, 0xfd, 0x2f, 0x5e, 0x74, 0xa9, 0x37, 0x99, 0xff, 0xf7, 0x4c, 0x2d, 0xa8, 0xcf, 0x51, 0xfd, 0x5b, 0xe7, 0x51, 0x14, 0x6d, 0xbc, 0x2f, 0x5b, 0xb9, 0x77, 0x85, 0xad, 0xb4, 0x72, 0x99, 0xad, 0x7b, 0x6c, 0x6a, 0xdf, 0x4d, 0xca, 0x2f, 0xef, 0x8b, 0x5e, 0x4b, 0xf3, 0xd9, 0xfd, 0xbd, 0x47, 0x1a, 0x72, 0xe2, 0x41, 0xd8, 0xfa, 0xa1, 0x25, 0x00, 0xa3, 0xfe, 0x12, 0xda, 0xf6, 0x16, 0xb3, 0xb3, 0x08, 0x02, 0xfd, 0x0a, 0x6a, 0xab, 0x85, 0x17, 0xd7, 0x0f, 0xf4, 0x6b, 0xdf, 0x8f, 0xe2, 0x05, 0xf4, 0x5b, 0x13, 0x26, 0xa9, 0xe2, 0x57, 0xb6, 0xda, 0x76, 0x17, 0x3c, 0x52, 0x13, 0x8d, 0x83, 0xc0, 0x2b, 0xe7, 0x2e, 0xbd, 0xb0, 0xde, 0x98, 0x4f, 0x7a, 0x95, 0xa1, 0x75, 0x19, 0x6e, 0xda, 0x19, 0xff, 0x7f, 0xdd, 0x70, 0x01, 0x12, 0x3c, 0x9e, 0xd7, 0xfe, 0xc3, 0x22, 0x39, 0xce, 0x4f, 0x86, 0xd8, 0x85, 0x40, 0x75, 0xd4, 0xfe, 0x93, 0x57, 0xbc, 0x9b, 0x01, 0xa4, 0x71, 0x35, 0x70, 0x9d, 0x62, 0x91, 0x47, 0x4e, 0x32, 0xa2, 0x76, 0x16, 0x06, 0xaf, 0xc7, 0xe3, 0xe5, 0xdc, 0x25, 0xac, 0xe7, 0x68, 0x25, 0x69, 0x0f, 0x97, 0x8d, 0x91, 0x32, 0x81, 0x23, 0xb1, 0x08, 0xf3, 0xa3, 0x2b, 0x3d, 0xfb, 0xcf, 0x99, 0x12, 0x0a, 0x59, 0xb9, 0xbb, 0x76, 0x16, 0x71, 0xa2, 0x0b, 0x0a, 0x5a, 0x6c, 0x37, 0x99, 0x9d, 0xe6, 0x3a, 0x05, 0x89, 0xf7, 0xc6, 0xfc, 0xf3, 0xfe, 0x36, 0x97, 0x77, 0x86, 0xe4, 0x7c, 0x48, 0x93, 0x0b, 0x26, 0x8e, 0x31, 0xe9, 0x22, 0xcc, 0xd3, 0xe0, 0x56, 0x29, 0xc8, 0x26, 0xe6, 0x1f, 0xa8, 0xb8, 0x93, 0x98, 0xec, 0xd9, 0x7c, 0x4a, 0x45, 0xd3, 0x71, 0x35, 0x9f, 0x14, 0xc1, 0x99, 0xd5, 0x21, 0x0b, 0xfa, 0x0f, 0xfb, 0x31, 0x7a, 0xa0, 0x70, 0x35, 0xb3, 0x9b, 0x1b, 0xe7, 0x65, 0xfd, 0xe3, 0x7d, 0x0b, 0xcc, 0x34, 0x4b, 0xf1, 0x5a, 0x9f, 0x19, 0xa4, 0x8f, 0xc8, 0x30, 0xf1, 0x87, 0x99, 0xc2, 0x75, 0x55, 0x2a, 0x34, 0xd7, 0x81, 0x9c, 0x54, 0x12, 0x82, 0x69, 0x5f, 0x8b, 0x01, 0xc1, 0x45, 0x95, 0xf5, 0xb1, 0x2d, 0x27, 0x0d, 0xa9, 0xc3, 0x93, 0x54, 0x2f, 0x57, 0x04, 0x7b, 0x20, 0xb7, 0xac, 0x33, 0x68, 0xb3, 0xef, 0x9a, 0x33, 0x95, 0x82, 0x9d, 0xfa, 0x2a, 0xb9, 0x88, 0x06, 0x04, 0x88, 0x51, 0x2c, 0x46, 0xdb, 0x83, 0xd7, 0x2f, 0xea, 0x1f, 0x0f, 0x24, 0xab, 0x03, 0xef, 0xb0, 0x61, 0xc7, 0x90, 0xdc, 0x78, 0x17, 0xf2, 0x9a, 0xab},
	/* This is a confirm1 packet, sequence number is 0x09f6, SSRC 0x12345678 */
	{0x10, 0x00, 0x09, 0xf6, 0x5a, 0x52, 0x54, 0x50, 0x12, 0x34, 0x56, 0x78, 0x50, 0x5a, 0x00, 0x13, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x72, 0x6d, 0x31, 0xb1, 0x21, 0xee, 0xe2, 0x67, 0xa4, 0xfd, 0xa6, 0x94, 0x24, 0x9a, 0x60, 0xf0, 0x2e, 0x5e, 0xd9, 0x33, 0xe1, 0xd8, 0x41, 0x54, 0xa3, 0x7c, 0xea, 0xe9, 0x61, 0xae, 0xf9, 0x19, 0x0d, 0xb3, 0x68, 0x68, 0x9e, 0xf8, 0x1a, 0x18, 0x91, 0x87, 0xc5, 0x6e, 0x5e, 0x2d, 0x5e, 0x32, 0xa2, 0xb2, 0x66, 0x31, 0xb8, 0xe5, 0x59, 0xc9, 0x10, 0xbb, 0xa0, 0x00, 0x6c, 0xee, 0x0c, 0x6d, 0xfb, 0xeb, 0x32, 0x85, 0xb6, 0x6e, 0x93},
	/* This is a confirm2 packet, sequence number is 0x02d2, SSRC 0x87654321 */
	{0x10, 0x00, 0x02, 0xd2, 0x5a, 0x52, 0x54, 0x50, 0x87, 0x65, 0x43, 0x21, 0x50, 0x5a, 0x00, 0x13, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x72, 0x6d, 0x32, 0x0f, 0xec, 0xfa, 0x4b, 0x45, 0x17, 0x9d, 0xb3, 0x92, 0x7d, 0x1c, 0x53, 0x86, 0x01, 0x12, 0xd9, 0x25, 0x48, 0xca, 0x18, 0xb9, 0x10, 0x95, 0x04, 0xb7, 0xc8, 0xee, 0x87, 0x2b, 0xec, 0x59, 0x39, 0x92, 0x96, 0x11, 0x73, 0xa6, 0x69, 0x2b, 0x11, 0xcd, 0x1d, 0xa1, 0x73, 0xb2, 0xc9, 0x29, 0x6f, 0x82, 0x32, 0x6a, 0x0a, 0x56, 0x40, 0x57, 0xfb, 0xac, 0xab, 0x20, 0xb8, 0xe2, 0xa9, 0x2c, 0x61, 0x6a, 0x05, 0xe8, 0xb5},
	/* This is a conf2ACK packet, sequence number 0x09f7, SSRC 0x12345678 */
	{0x10, 0x00, 0x09, 0xf7, 0x5a, 0x52, 0x54, 0x50, 0x12, 0x34, 0x56, 0x78, 0x50, 0x5a, 0x00, 0x03, 0x43, 0x6f, 0x6e, 0x66, 0x32, 0x41, 0x43, 0x4b, 0x23, 0xc1, 0x1b, 0x45},
};

/* Hash images for both sides */
uint8_t H12345678[4][32] = {
	{0xbb, 0xbf, 0x7e, 0xb1, 0x14, 0xd5, 0xd4, 0x0c, 0x6b, 0xb0, 0x79, 0x58, 0x19, 0xc1, 0xd0, 0x83, 0xc9, 0xe1, 0xf4, 0x2e, 0x11, 0xcd, 0x7e, 0xc3, 0xaa, 0xd8, 0xb9, 0x17, 0xe6, 0xb5, 0x9e, 0x86},
	{0x28, 0x7c, 0x28, 0xe4, 0xd7, 0x3d, 0x14, 0x39, 0xb5, 0x6d, 0x1c, 0x47, 0x9d, 0x59, 0x0a, 0xf2, 0x10, 0x33, 0xde, 0x6b, 0xd5, 0x2b, 0xfb, 0x26, 0xa5, 0x87, 0x4d, 0xe9, 0x20, 0x6b, 0x9f, 0xdd},
	{0x70, 0x12, 0xef, 0x2e, 0x85, 0x2f, 0xfc, 0x84, 0xb8, 0x8d, 0xcc, 0x03, 0xd7, 0x8f, 0x53, 0x01, 0x63, 0xfb, 0xd3, 0xb0, 0x2d, 0xbb, 0x9e, 0x98, 0x22, 0x0d, 0xe3, 0xe3, 0x64, 0x25, 0x04, 0x0f},
	{0xe8, 0xd5, 0x26, 0xc1, 0x3a, 0x0c, 0x4c, 0x6a, 0xce, 0x18, 0xaa, 0xc7, 0xc4, 0xa4, 0x07, 0x0e, 0x65, 0x7a, 0x4d, 0xca, 0x78, 0xf2, 0xcc, 0xcd, 0x20, 0x50, 0x38, 0x73, 0xe9, 0x7e, 0x08, 0x29}
};

uint8_t H87654321[4][32] = {
	{0x09, 0x02, 0xcc, 0x13, 0xc4, 0x84, 0x03, 0x31, 0x68, 0x91, 0x05, 0x4d, 0xe0, 0x6d, 0xf4, 0xc9, 0x6a, 0xb5, 0xbe, 0x82, 0xe8, 0x37, 0x33, 0xb7, 0xa9, 0xce, 0xbe, 0xb5, 0x42, 0xaa, 0x54, 0xba},
	{0x9e, 0xb2, 0xa5, 0x8b, 0xe8, 0x96, 0x37, 0xf5, 0x5a, 0x41, 0x34, 0xb2, 0xec, 0xda, 0x84, 0x95, 0xf0, 0xf8, 0x9b, 0xab, 0x61, 0x4f, 0x7c, 0x9e, 0x56, 0xb7, 0x3b, 0xd3, 0x46, 0xba, 0xbe, 0x9a},
	{0xd9, 0xff, 0x14, 0x8b, 0x34, 0xaa, 0x69, 0xe9, 0x33, 0xc1, 0x62, 0xe6, 0x6b, 0xe8, 0xcd, 0x9d, 0xe3, 0x0f, 0xb7, 0x6a, 0xe8, 0x6a, 0x62, 0x2b, 0xcb, 0xe4, 0x6b, 0x91, 0x05, 0xc7, 0xc8, 0x7e},
	{0x8d, 0x0f, 0x5a, 0x20, 0x79, 0x97, 0x42, 0x01, 0x99, 0x45, 0x45, 0xf7, 0x0e, 0x31, 0x06, 0xe1, 0x05, 0xc0, 0xb9, 0x24, 0xe9, 0xc9, 0x78, 0xc7, 0x38, 0xf5, 0x97, 0x48, 0xef, 0x42, 0x6a, 0x3e}
};

/* mac and zrtp keys */
uint8_t mackeyi[32] = {0xdc, 0x47, 0xe1, 0xc7, 0x48, 0x11, 0xb1, 0x54, 0x14, 0x2a, 0x91, 0x29, 0x9f, 0xa4, 0x8b, 0x45, 0x87, 0x16, 0x8d, 0x3a, 0xe6, 0xb0, 0x0c, 0x08, 0x4f, 0xa5, 0x48, 0xd5, 0x96, 0x67, 0x1a, 0x1b};
uint8_t mackeyr[32] = {0x3a, 0xa5, 0x22, 0x43, 0x26, 0x13, 0x8f, 0xd6, 0x54, 0x59, 0x40, 0xb8, 0x5c, 0xf4, 0x0f, 0x0c, 0xbc, 0x9c, 0x4f, 0x7d, 0x55, 0xeb, 0x4b, 0xa5, 0x1e, 0x1c, 0x42, 0xd0, 0x5e, 0xac, 0x12, 0x06};
uint8_t zrtpkeyi[16] = {0x22, 0xf6, 0xea, 0xaa, 0xa4, 0xad, 0x53, 0x30, 0x71, 0x97, 0xcc, 0x68, 0x6b, 0xb0, 0xcb, 0x55};
uint8_t zrtpkeyr[16] = {0x09, 0x50, 0xcd, 0x9e, 0xc2, 0x78, 0x54, 0x31, 0x93, 0x2e, 0x99, 0x31, 0x15, 0x58, 0xd0, 0x2a};

void test_parser(void) {
	int i, retval;
	bzrtpPacket_t *zrtpPacket;
	
	/* Create zrtp Context to use H0-H3 chains and others */
	bzrtpContext_t *context87654321 = bzrtp_createBzrtpContext(0x87654321);
	bzrtpContext_t *context12345678 = bzrtp_createBzrtpContext(0x12345678);


	/* replace created H by the patterns one to be able to generate the correct packet */
	memcpy (context12345678->channelContext[0]->selfH[0], H12345678[0], 32);
	memcpy (context12345678->channelContext[0]->selfH[1], H12345678[1], 32);
	memcpy (context12345678->channelContext[0]->selfH[2], H12345678[2], 32);
	memcpy (context12345678->channelContext[0]->selfH[3], H12345678[3], 32);
	memcpy (context87654321->channelContext[0]->selfH[0], H87654321[0], 32);
	memcpy (context87654321->channelContext[0]->selfH[1], H87654321[1], 32);
	memcpy (context87654321->channelContext[0]->selfH[2], H87654321[2], 32);
	memcpy (context87654321->channelContext[0]->selfH[3], H87654321[3], 32);

	/* preset the key agreement algo in the contexts */
	context87654321->channelContext[0]->keyAgreementAlgo = ZRTP_KEYAGREEMENT_DH3k;
	context12345678->channelContext[0]->keyAgreementAlgo = ZRTP_KEYAGREEMENT_DH3k;
	context87654321->channelContext[0]->cipherAlgo = ZRTP_CIPHER_AES1;
	context12345678->channelContext[0]->cipherAlgo = ZRTP_CIPHER_AES1;
	context87654321->channelContext[0]->hashAlgo = ZRTP_HASH_S256;
	context12345678->channelContext[0]->hashAlgo = ZRTP_HASH_S256;

	updateCryptoFunctionPointers(context87654321->channelContext[0]);
	updateCryptoFunctionPointers(context12345678->channelContext[0]);

	/* set the zrtp and mac keys */
	context87654321->channelContext[0]->mackeyi = (uint8_t *)malloc(32);
	context12345678->channelContext[0]->mackeyi = (uint8_t *)malloc(32);
	context87654321->channelContext[0]->mackeyr = (uint8_t *)malloc(32);
	context12345678->channelContext[0]->mackeyr = (uint8_t *)malloc(32);

	context87654321->channelContext[0]->zrtpkeyi = (uint8_t *)malloc(16);
	context12345678->channelContext[0]->zrtpkeyi = (uint8_t *)malloc(16);
	context87654321->channelContext[0]->zrtpkeyr = (uint8_t *)malloc(16);
	context12345678->channelContext[0]->zrtpkeyr = (uint8_t *)malloc(16);

	memcpy(context12345678->channelContext[0]->mackeyi, mackeyi, 32);
	memcpy(context12345678->channelContext[0]->mackeyr, mackeyr, 32);
	memcpy(context12345678->channelContext[0]->zrtpkeyi, zrtpkeyi, 16);
	memcpy(context12345678->channelContext[0]->zrtpkeyr, zrtpkeyr, 16);
	memcpy(context87654321->channelContext[0]->mackeyi, mackeyi, 32);
	memcpy(context87654321->channelContext[0]->mackeyr, mackeyr, 32);
	memcpy(context87654321->channelContext[0]->zrtpkeyi, zrtpkeyi, 16);
	memcpy(context87654321->channelContext[0]->zrtpkeyr, zrtpkeyr, 16);

	/* set the role: 87654321 is initiator in our exchange pattern */
	context12345678->channelContext[0]->role = RESPONDER; 

	for (i=0; i<TEST_PACKET_NUMBER; i++) {
		uint8_t freePacketFlag = 1;
		/* parse a packet string from patterns */
		zrtpPacket = bzrtp_packetCheck(patternZRTPPackets[i], patternZRTPMetaData[i][0], (patternZRTPMetaData[i][1])-1, &retval);
		retval +=  bzrtp_packetParser((patternZRTPMetaData[i][2]==0x87654321)?context12345678:context87654321, (patternZRTPMetaData[i][2]==0x87654321)?context12345678->channelContext[0]:context87654321->channelContext[0], patternZRTPPackets[i], patternZRTPMetaData[i][0], zrtpPacket);
		/*printf("parsing Ret val is %x index is %d\n", retval, i);*/
		/* We must store some packets in the context if we want to be able to parse further packets */
		if (zrtpPacket->messageType==MSGTYPE_HELLO) {
			if (patternZRTPMetaData[i][2]==0x87654321) {
				context12345678->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID] = zrtpPacket;
			} else {
				context87654321->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID] = zrtpPacket;
			}
			freePacketFlag = 0;
		}
		if (zrtpPacket->messageType==MSGTYPE_COMMIT) {
			if (patternZRTPMetaData[i][2]==0x87654321) {
				context12345678->channelContext[0]->peerPackets[COMMIT_MESSAGE_STORE_ID] = zrtpPacket;
			} else {
				context87654321->channelContext[0]->peerPackets[COMMIT_MESSAGE_STORE_ID] = zrtpPacket;
			}
			freePacketFlag = 0;
		}
		if (zrtpPacket->messageType==MSGTYPE_DHPART1 || zrtpPacket->messageType==MSGTYPE_DHPART2) {
			if (patternZRTPMetaData[i][2]==0x87654321) {
				context12345678->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID] = zrtpPacket;
			} else {
				context87654321->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID] = zrtpPacket;
			}
			freePacketFlag = 0;
		}
		/* free the packet string as will be created again by the packetBuild function and might have been copied by packetParser */
		free(zrtpPacket->packetString);
		/* build a packet string from the parser packet*/
		retval = bzrtp_packetBuild((patternZRTPMetaData[i][2]==0x12345678)?context12345678:context87654321, (patternZRTPMetaData[i][2]==0x12345678)?context12345678->channelContext[0]:context87654321->channelContext[0], zrtpPacket, patternZRTPMetaData[i][1]);
		/* if (retval ==0) {
			packetDump(zrtpPacket, 1);
		} else {
			printf("Ret val is %x index is %d\n", retval, i);
		}*/

		/* check they are the same */
		if (zrtpPacket->packetString != NULL) {
			CU_ASSERT_TRUE(memcmp(zrtpPacket->packetString, patternZRTPPackets[i], patternZRTPMetaData[i][0]) == 0);
		} else {
			CU_FAIL("Unable to build packet");
		}

		if (freePacketFlag == 1) {
			bzrtp_freeZrtpPacket(zrtpPacket);
		}
	}

	bzrtp_destroyBzrtpContext(context87654321, 0x87654321);
	bzrtp_destroyBzrtpContext(context12345678, 0x12345678);

}

/* context structure mainly used by statemachine test, but also needed by parserComplete to get the zid Filename */
typedef struct my_Context_struct {
	unsigned char nom[30]; /* nom du contexte */
	bzrtpContext_t *peerContext;
	bzrtpChannelContext_t *peerChannelContext;
	char zidFilename[80]; /* nom du fichier de cache */
} my_Context_t;

int floadAlice(void *clientData, uint8_t **output, uint32_t *size) {
	/* get filename from ClientData */
	my_Context_t *clientContext = (my_Context_t *)clientData;
	char *filename = clientContext->zidFilename; 
	FILE *ALICECACHE = fopen(filename, "r+");
	fseek(ALICECACHE, 0L, SEEK_END);  /* Position to end of file */
  	*size = ftell(ALICECACHE);     /* Get file length */
  	rewind(ALICECACHE);               /* Back to start of file */
	*output = (uint8_t *)malloc(*size*sizeof(uint8_t));
	fread(*output, 1, *size, ALICECACHE);
	fclose(ALICECACHE);
	return *size;
}

int fwriteAlice(void *clientData, uint8_t *input, uint32_t size) {
	/* get filename from ClientData */
	my_Context_t *clientContext = (my_Context_t *)clientData;
	char *filename = clientContext->zidFilename; 

	FILE *ALICECACHE = fopen(filename, "w+");
	int retval = fwrite(input, 1, size, ALICECACHE);
	fclose(ALICECACHE);
	return retval;
}

int floadBob(void *clientData, uint8_t **output, uint32_t *size) {
	/* get filename from ClientData */
	my_Context_t *clientContext = (my_Context_t *)clientData;
	char *filename = clientContext->zidFilename; 

	FILE *BOBCACHE = fopen(filename, "r+");
	fseek(BOBCACHE, 0L, SEEK_END);  /* Position to end of file */
  	*size = ftell(BOBCACHE);     /* Get file length */
  	rewind(BOBCACHE);               /* Back to start of file */
	*output = (uint8_t *)malloc(*size*sizeof(uint8_t));
	fread(*output, 1, *size, BOBCACHE);
	fclose(BOBCACHE);
	return *size;
}


int fwriteBob(void *clientData, uint8_t *input, uint32_t size) {
	/* get filename from ClientData */
	my_Context_t *clientContext = (my_Context_t *)clientData;
	char *filename = clientContext->zidFilename; 

	FILE *BOBCACHE = fopen(filename, "w+");
	int retval = fwrite(input, 1, size, BOBCACHE);
	fclose(BOBCACHE);
	return retval;
}

void test_parserComplete() {

	int retval;
	/* alice's maintained packet */
	bzrtpPacket_t *alice_Hello, *alice_HelloFromBob, *alice_HelloACK, *alice_HelloACKFromBob;
	/* bob's maintained packet */
	bzrtpPacket_t *bob_Hello, *bob_HelloFromAlice, *bob_HelloACK, *bob_HelloACKFromAlice;
	/* Create zrtp Context */
	bzrtpContext_t *contextAlice = bzrtp_createBzrtpContext(0x12345678); /* Alice's SSRC of main channel is 12345678 */
	bzrtpContext_t *contextBob = bzrtp_createBzrtpContext(0x87654321); /* Bob's SSRC of main channel is 87654321 */

	/* Create the client context, used for zidFilename only */
	my_Context_t clientContextAlice;
	my_Context_t clientContextBob;
	memcpy(clientContextAlice.zidFilename, "test/ZIDAlice.txt", 18);
	memcpy(clientContextBob.zidFilename, "test/ZIDBob.txt", 16);

	/* attach the clientContext to the bzrtp Context */
	retval = bzrtp_setClientData(contextAlice, 0x12345678, (void *)&clientContextAlice);
	retval += bzrtp_setClientData(contextBob, 0x87654321, (void *)&clientContextBob);

	/* set the cache related callback functions */
	bzrtp_setCallback(contextAlice, (int (*)())floadAlice, ZRTP_CALLBACK_LOADCACHE);
	bzrtp_setCallback(contextAlice, (int (*)())fwriteAlice, ZRTP_CALLBACK_WRITECACHE);

	bzrtp_setCallback(contextBob, (int (*)())floadBob, ZRTP_CALLBACK_LOADCACHE);
	bzrtp_setCallback(contextBob, (int (*)())fwriteBob, ZRTP_CALLBACK_WRITECACHE);

	printf ("Init the contexts\n");
	/* end the context init */
	bzrtp_initBzrtpContext(contextAlice);
	bzrtp_initBzrtpContext(contextBob);

	/* now create Alice and BOB Hello packet */
	alice_Hello = bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[0], MSGTYPE_HELLO, &retval);
	if (bzrtp_packetBuild(contextAlice, contextAlice->channelContext[0], alice_Hello, contextAlice->channelContext[0]->selfSequenceNumber) ==0) {
		contextAlice->channelContext[0]->selfSequenceNumber++;
		contextAlice->channelContext[0]->selfPackets[HELLO_MESSAGE_STORE_ID] = alice_Hello;
	}
	bob_Hello = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[0], MSGTYPE_HELLO, &retval);
	if (bzrtp_packetBuild(contextBob, contextBob->channelContext[0], bob_Hello, contextBob->channelContext[0]->selfSequenceNumber) ==0) {
		contextBob->channelContext[0]->selfSequenceNumber++;
		contextBob->channelContext[0]->selfPackets[HELLO_MESSAGE_STORE_ID] = bob_Hello;
	}

	/* now send Alice Hello's to Bob and vice-versa, so they parse them */
	alice_HelloFromBob = bzrtp_packetCheck(bob_Hello->packetString, bob_Hello->messageLength+16, contextAlice->channelContext[0]->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextAlice, contextAlice->channelContext[0], bob_Hello->packetString, bob_Hello->messageLength+16, alice_HelloFromBob);
	printf ("Alice parsing returns %x\n", retval);
	if (retval==0) {
		contextAlice->channelContext[0]->peerSequenceNumber = alice_HelloFromBob->sequenceNumber;
		/* save bob's Hello packet in Alice's context */
		contextAlice->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID] = alice_HelloFromBob;

		/* determine crypto Algo to use */
		bzrtpHelloMessage_t *alice_HelloFromBob_message = (bzrtpHelloMessage_t *)alice_HelloFromBob->messageData;
		retval = crypoAlgoAgreement(contextAlice, contextAlice->channelContext[0], contextAlice->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData);
		if (retval == 0) {
			printf ("Alice selected algo %x\n", contextAlice->channelContext[0]->keyAgreementAlgo);
			memcpy(contextAlice->peerZID, alice_HelloFromBob_message->ZID, 12);
		}

		/* check if the peer accept MultiChannel */
		int i;
		for (i=0; i<alice_HelloFromBob_message->kc; i++) {
			if (alice_HelloFromBob_message->supportedKeyAgreement[i] == ZRTP_KEYAGREEMENT_Mult) {
				contextAlice->peerSupportMultiChannel = 1;
			}
		}
	}

	bob_HelloFromAlice = bzrtp_packetCheck(alice_Hello->packetString, alice_Hello->messageLength+16, contextBob->channelContext[0]->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextBob, contextBob->channelContext[0], alice_Hello->packetString, alice_Hello->messageLength+16, bob_HelloFromAlice);
	printf ("Bob parsing returns %x\n", retval);
	if (retval==0) {
		contextBob->channelContext[0]->peerSequenceNumber = bob_HelloFromAlice->sequenceNumber;
		/* save alice's Hello packet in bob's context */
		contextBob->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID] = bob_HelloFromAlice;

		/* determine crypto Algo to use */
		bzrtpHelloMessage_t *bob_HelloFromAlice_message = (bzrtpHelloMessage_t *)bob_HelloFromAlice->messageData;
		retval = crypoAlgoAgreement(contextBob, contextBob->channelContext[0], contextBob->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID]->messageData);
		if (retval == 0) {
			printf ("Bob selected algo %x\n", contextBob->channelContext[0]->keyAgreementAlgo);
			memcpy(contextBob->peerZID, bob_HelloFromAlice_message->ZID, 12);
		}

		/* check if the peer accept MultiChannel */
		int i;
		for (i=0; i<bob_HelloFromAlice_message->kc; i++) {
			if (bob_HelloFromAlice_message->supportedKeyAgreement[i] == ZRTP_KEYAGREEMENT_Mult) {
				contextBob->peerSupportMultiChannel = 1;
			}
		}
	}

	/* update context with hello message information : H3  and compute initiator and responder's shared secret Hashs */
	bzrtpHelloMessage_t *alice_HelloFromBob_message = (bzrtpHelloMessage_t *)alice_HelloFromBob->messageData;
	memcpy(contextAlice->channelContext[0]->peerH[3], alice_HelloFromBob_message->H3, 32);
	bzrtpHelloMessage_t *bob_HelloFromAlice_message = (bzrtpHelloMessage_t *)bob_HelloFromAlice->messageData;
	memcpy(contextBob->channelContext[0]->peerH[3], bob_HelloFromAlice_message->H3, 32);

	/* get the secrets associated to peer ZID */
	bzrtp_getPeerAssociatedSecretsHash(contextAlice, alice_HelloFromBob_message->ZID);
	bzrtp_getPeerAssociatedSecretsHash(contextBob, bob_HelloFromAlice_message->ZID);

	/* compute the initiator hashed secret as in rfc section 4.3.1 */
	if (contextAlice->cachedSecret.rs1!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.rs1, contextAlice->cachedSecret.rs1Length, (uint8_t *)"Initiator", 9, 8, contextAlice->initiatorCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.rs1ID, 8);
	}

	if (contextAlice->cachedSecret.rs2!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.rs2, contextAlice->cachedSecret.rs2Length, (uint8_t *)"Initiator", 9, 8, contextAlice->initiatorCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.rs2ID, 8);
	}

	if (contextAlice->cachedSecret.auxsecret!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.auxsecret, contextAlice->cachedSecret.auxsecretLength, contextAlice->channelContext[0]->selfH[3], 32, 8, contextAlice->channelContext[0]->initiatorAuxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->channelContext[0]->initiatorAuxsecretID, 8);
	}

	if (contextAlice->cachedSecret.pbxsecret!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.pbxsecret, contextAlice->cachedSecret.pbxsecretLength, (uint8_t *)"Initiator", 9, 8, contextAlice->initiatorCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->initiatorCachedSecretHash.pbxsecretID, 8);
	}

	if (contextAlice->cachedSecret.rs1!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.rs1, contextAlice->cachedSecret.rs1Length, (uint8_t *)"Responder", 9, 8, contextAlice->responderCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->responderCachedSecretHash.rs1ID, 8);
	}

	if (contextAlice->cachedSecret.rs2!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.rs2, contextAlice->cachedSecret.rs2Length, (uint8_t *)"Responder", 9, 8, contextAlice->responderCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->responderCachedSecretHash.rs2ID, 8);
	}

	if (contextAlice->cachedSecret.auxsecret!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.auxsecret, contextAlice->cachedSecret.auxsecretLength, contextAlice->channelContext[0]->peerH[3], 32, 8, contextAlice->channelContext[0]->responderAuxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->channelContext[0]->responderAuxsecretID, 8);
	}

	if (contextAlice->cachedSecret.pbxsecret!=NULL) {
		contextAlice->channelContext[0]->hmacFunction(contextAlice->cachedSecret.pbxsecret, contextAlice->cachedSecret.pbxsecretLength, (uint8_t *)"Responder", 9, 8, contextAlice->responderCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextAlice->RNGContext, contextAlice->responderCachedSecretHash.pbxsecretID, 8);
	}


	/* Bob hashes*/
	if (contextBob->cachedSecret.rs1!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.rs1, contextBob->cachedSecret.rs1Length, (uint8_t *)"Initiator", 9, 8, contextBob->initiatorCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.rs1ID, 8);
	}

	if (contextBob->cachedSecret.rs2!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.rs2, contextBob->cachedSecret.rs2Length, (uint8_t *)"Initiator", 9, 8, contextBob->initiatorCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.rs2ID, 8);
	}

	if (contextBob->cachedSecret.auxsecret!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.auxsecret, contextBob->cachedSecret.auxsecretLength, contextBob->channelContext[0]->selfH[3], 32, 8, contextBob->channelContext[0]->initiatorAuxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->channelContext[0]->initiatorAuxsecretID, 8);
	}

	if (contextBob->cachedSecret.pbxsecret!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.pbxsecret, contextBob->cachedSecret.pbxsecretLength, (uint8_t *)"Initiator", 9, 8, contextBob->initiatorCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->initiatorCachedSecretHash.pbxsecretID, 8);
	}

	if (contextBob->cachedSecret.rs1!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.rs1, contextBob->cachedSecret.rs1Length, (uint8_t *)"Responder", 9, 8, contextBob->responderCachedSecretHash.rs1ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->responderCachedSecretHash.rs1ID, 8);
	}

	if (contextBob->cachedSecret.rs2!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.rs2, contextBob->cachedSecret.rs2Length, (uint8_t *)"Responder", 9, 8, contextBob->responderCachedSecretHash.rs2ID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->responderCachedSecretHash.rs2ID, 8);
	}

	if (contextBob->cachedSecret.auxsecret!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.auxsecret, contextBob->cachedSecret.auxsecretLength, contextBob->channelContext[0]->peerH[3], 32, 8, contextBob->channelContext[0]->responderAuxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->channelContext[0]->responderAuxsecretID, 8);
	}

	if (contextBob->cachedSecret.pbxsecret!=NULL) {
		contextBob->channelContext[0]->hmacFunction(contextBob->cachedSecret.pbxsecret, contextBob->cachedSecret.pbxsecretLength, (uint8_t *)"Responder", 9, 8, contextBob->responderCachedSecretHash.pbxsecretID);
	} else { /* we have no secret, generate a random */
		bzrtpCrypto_getRandom(contextBob->RNGContext, contextBob->responderCachedSecretHash.pbxsecretID, 8);
	}

	/* dump alice's packet on both sides */
	printf ("\nAlice original Packet is \n");
	packetDump(alice_Hello, 1);
	printf ("\nBob's parsed Alice Packet is \n");
	packetDump(bob_HelloFromAlice, 0);

	/* Create the DHPart2 packet (that we then may change to DHPart1 if we ended to be the responder) */
	bzrtpPacket_t *alice_selfDHPart = bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[0], MSGTYPE_DHPART2, &retval); 
	retval += bzrtp_packetBuild(contextAlice, contextAlice->channelContext[0], alice_selfDHPart, 0); /* we don't care now about sequence number as we just need to build the message to be able to insert a hash of it into the commit packet */
	if (retval == 0) { /* ok, insert it in context as we need it to build the commit packet */
		contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID] = alice_selfDHPart;
	} else {
		printf ("Alice building DHPart packet returns %x\n", retval);
	}
	bzrtpPacket_t *bob_selfDHPart = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[0], MSGTYPE_DHPART2, &retval); 
	retval +=bzrtp_packetBuild(contextBob, contextBob->channelContext[0], bob_selfDHPart, 0); /* we don't care now about sequence number as we just need to build the message to be able to insert a hash of it into the commit packet */
	if (retval == 0) { /* ok, insert it in context as we need it to build the commit packet */
		contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID] = bob_selfDHPart;
	} else {
		printf ("Bob building DHPart packet returns %x\n", retval);
	}
	printf("Alice DHPart packet:\n");
	packetDump(alice_selfDHPart,0);
	printf("Bob DHPart packet:\n");
	packetDump(bob_selfDHPart,0);

	/* respond to HELLO packet with an HelloACK - 1 create packets */
	alice_HelloACK = bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[0], MSGTYPE_HELLOACK, &retval);
	retval += bzrtp_packetBuild(contextAlice, contextAlice->channelContext[0], alice_HelloACK, contextAlice->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->channelContext[0]->selfSequenceNumber++;
	} else {
		printf("Alice building HelloACK return %x\n", retval);
	}

	bob_HelloACK = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[0], MSGTYPE_HELLOACK, &retval);
	retval += bzrtp_packetBuild(contextBob, contextBob->channelContext[0], bob_HelloACK, contextBob->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextBob->channelContext[0]->selfSequenceNumber++;
	} else {
		printf("Bob building HelloACK return %x\n", retval);
	}

	/* exchange the HelloACK */
	alice_HelloACKFromBob = bzrtp_packetCheck(bob_HelloACK->packetString, bob_HelloACK->messageLength+16, contextAlice->channelContext[0]->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextAlice, contextAlice->channelContext[0], bob_HelloACK->packetString, bob_HelloACK->messageLength+16, alice_HelloACKFromBob);
	printf ("Alice parsing Hello ACK returns %x\n", retval);
	if (retval==0) {
		contextAlice->channelContext[0]->peerSequenceNumber = alice_HelloACKFromBob->sequenceNumber;
	}

	bob_HelloACKFromAlice = bzrtp_packetCheck(alice_HelloACK->packetString, alice_HelloACK->messageLength+16, contextBob->channelContext[0]->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextBob, contextBob->channelContext[0], alice_HelloACK->packetString, alice_HelloACK->messageLength+16, bob_HelloACKFromAlice);
	printf ("Bob parsing Hello ACK returns %x\n", retval);
	if (retval==0) {
		contextBob->channelContext[0]->peerSequenceNumber = bob_HelloACKFromAlice->sequenceNumber;
	}


	/* now build the commit message (both Alice and Bob will send it, then use the mechanism of rfc section 4.2 to determine who will be the initiator)*/
	bzrtpPacket_t *alice_Commit = bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[0], MSGTYPE_COMMIT, &retval);
	retval += bzrtp_packetBuild(contextAlice, contextAlice->channelContext[0], alice_Commit, contextAlice->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->channelContext[0]->selfSequenceNumber++;
		contextAlice->channelContext[0]->selfPackets[COMMIT_MESSAGE_STORE_ID] = alice_Commit;
	}
	printf("Alice building Commit return %x\n", retval);
	
	bzrtpPacket_t *bob_Commit = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[0], MSGTYPE_COMMIT, &retval);
	retval += bzrtp_packetBuild(contextBob, contextBob->channelContext[0], bob_Commit, contextBob->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextBob->channelContext[0]->selfSequenceNumber++;
		contextBob->channelContext[0]->selfPackets[COMMIT_MESSAGE_STORE_ID] = bob_Commit;
	}
	printf("Bob building Commit return %x\n", retval);


	/* and exchange the commits */
	bzrtpPacket_t *bob_CommitFromAlice = bzrtp_packetCheck(alice_Commit->packetString, alice_Commit->messageLength+16, contextBob->channelContext[0]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextBob, contextBob->channelContext[0], alice_Commit->packetString, alice_Commit->messageLength+16, bob_CommitFromAlice);
	printf ("Bob parsing Commit returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		bzrtpCommitMessage_t *bob_CommitFromAlice_message = (bzrtpCommitMessage_t *)bob_CommitFromAlice->messageData;
		contextBob->channelContext[0]->peerSequenceNumber = bob_CommitFromAlice->sequenceNumber;
		memcpy(contextBob->channelContext[0]->peerH[2], bob_CommitFromAlice_message->H2, 32);
		contextBob->channelContext[0]->peerPackets[COMMIT_MESSAGE_STORE_ID] = bob_CommitFromAlice;
	}
	packetDump(bob_CommitFromAlice, 0);

	bzrtpPacket_t *alice_CommitFromBob = bzrtp_packetCheck(bob_Commit->packetString, bob_Commit->messageLength+16, contextAlice->channelContext[0]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, contextAlice->channelContext[0], bob_Commit->packetString, bob_Commit->messageLength+16, alice_CommitFromBob);
	printf ("Alice parsing Commit returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->channelContext[0]->peerSequenceNumber = alice_CommitFromBob->sequenceNumber;
		/* Alice will be the initiator (commit contention not implemented in this test) so just discard bob's commit */
		/*bzrtpCommirMessage_t *alice_CommitFromBob_message = (bzrtpCommitMessage_t *)alice_CommitFromBob->messageData;
		memcpy(contextAlice->channelContext[0]->peerH[2], alice_CommitFromBob_message->H2, 32); 
		contextAlice->channelContext[0]->peerPackets[COMMIT_MESSAGE_STORE_ID] = alice_CommitFromBob;*/
	}
	packetDump(alice_CommitFromBob, 0);

	/* Now determine who shall be the initiator : rfc section 4.2 */
	/* select the one with the lowest value of hvi */
	/* for test purpose, we will set Alice as the initiator */
	contextBob->channelContext[0]->role = RESPONDER;

	/* Bob (responder) shall update his selected algo list to match Alice selection */
	/* no need to do this here as we have the same selection */

	/* Bob is the responder, rebuild his DHPart packet to be responder and not initiator : */
	/* as responder, bob must also swap his aux shared secret between responder and initiator as they are computed using the H3 and not a constant string */
	uint8_t tmpBuffer[8];
	memcpy(tmpBuffer, contextBob->channelContext[0]->initiatorAuxsecretID, 8);
	memcpy(contextBob->channelContext[0]->initiatorAuxsecretID, contextBob->channelContext[0]->responderAuxsecretID, 8);
	memcpy(contextBob->channelContext[0]->responderAuxsecretID, tmpBuffer, 8);

	contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageType = MSGTYPE_DHPART1; /* we are now part 1*/
	bzrtpDHPartMessage_t *bob_DHPart1 = (bzrtpDHPartMessage_t *)contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageData;
	/* change the shared secret ID to the responder one (we set them by default to the initiator's one) */
	memcpy(bob_DHPart1->rs1ID, contextBob->responderCachedSecretHash.rs1ID, 8);
	memcpy(bob_DHPart1->rs2ID, contextBob->responderCachedSecretHash.rs2ID, 8);
	memcpy(bob_DHPart1->auxsecretID, contextBob->channelContext[0]->responderAuxsecretID, 8);
	memcpy(bob_DHPart1->pbxsecretID, contextBob->responderCachedSecretHash.pbxsecretID, 8);

	retval +=bzrtp_packetBuild(contextBob, contextBob->channelContext[0], contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID],contextBob->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextBob->channelContext[0]->selfSequenceNumber++;
	}
	printf("Bob building DHPart1 return %x\n", retval);


	/* Alice parse bob's DHPart1 message */
	bzrtpPacket_t *alice_DHPart1FromBob = bzrtp_packetCheck(contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+16, contextAlice->channelContext[0]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, contextAlice->channelContext[0], contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+16, alice_DHPart1FromBob);
	printf ("Alice parsing DHPart1 returns %x\n", retval);
	bzrtpDHPartMessage_t *alice_DHPart1FromBob_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->channelContext[0]->peerSequenceNumber = alice_DHPart1FromBob->sequenceNumber;
		alice_DHPart1FromBob_message = (bzrtpDHPartMessage_t *)alice_DHPart1FromBob->messageData;
		memcpy(contextAlice->channelContext[0]->peerH[1], alice_DHPart1FromBob_message->H1, 32);
		contextAlice->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID] = alice_DHPart1FromBob;
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
		if (memcmp(contextAlice->channelContext[0]->responderAuxsecretID, alice_DHPart1FromBob_message->auxsecretID,8) != 0) {
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
	bzrtp_packetUpdateSequenceNumber(contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID], contextAlice->channelContext[0]->selfSequenceNumber);
	contextAlice->channelContext[0]->selfSequenceNumber++;

	/* Bob parse Alice's DHPart2 message */
	bzrtpPacket_t *bob_DHPart2FromAlice = bzrtp_packetCheck(contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+16, contextBob->channelContext[0]->peerSequenceNumber, &retval);
	printf ("Bob checking DHPart2 returns %x\n", retval);
	retval += bzrtp_packetParser(contextBob, contextBob->channelContext[0], contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString, contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength+16, bob_DHPart2FromAlice);
	printf ("Bob parsing DHPart2 returns %x\n", retval);
	bzrtpDHPartMessage_t *bob_DHPart2FromAlice_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextBob->channelContext[0]->peerSequenceNumber = bob_DHPart2FromAlice->sequenceNumber;
		bob_DHPart2FromAlice_message = (bzrtpDHPartMessage_t *)bob_DHPart2FromAlice->messageData;
		memcpy(contextBob->channelContext[0]->peerH[1], bob_DHPart2FromAlice_message->H1, 32);
		contextBob->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID] = bob_DHPart2FromAlice;
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
		if (memcmp(contextBob->channelContext[0]->initiatorAuxsecretID, bob_DHPart2FromAlice_message->auxsecretID,8) != 0) {
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
	uint16_t totalHashDataLength = bob_Hello->messageLength + alice_Commit->messageLength + contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength + alice_selfDHPart->messageLength;
	uint8_t *dataToHash = (uint8_t *)malloc(totalHashDataLength*sizeof(uint8_t));
	uint16_t hashDataIndex = 0;
	/* get all data from Alice */
	memcpy(dataToHash, contextAlice->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextAlice->channelContext[0]->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextAlice->channelContext[0]->selfPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->channelContext[0]->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextAlice->channelContext[0]->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextAlice->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextAlice->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength);

	uint8_t alice_totalHash[32]; /* Note : actual length of hash depends on the choosen algo */
	contextAlice->channelContext[0]->hashFunction(dataToHash, totalHashDataLength, 32, alice_totalHash);

	/* get all data from Bob */
	hashDataIndex = 0;
	memcpy(dataToHash, contextBob->channelContext[0]->selfPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->channelContext[0]->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextBob->channelContext[0]->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextBob->channelContext[0]->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->channelContext[0]->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextBob->channelContext[0]->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextBob->channelContext[0]->selfPackets[DHPART_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextBob->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->channelContext[0]->peerPackets[DHPART_MESSAGE_STORE_ID]->messageLength);

	uint8_t bob_totalHash[32]; /* Note : actual length of hash depends on the choosen algo */
	contextBob->channelContext[0]->hashFunction(dataToHash, totalHashDataLength, 32, bob_totalHash);
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
	contextAlice->channelContext[0]->KDFContextLength = 24+32;/* actual depends on selected hash length*/
	contextAlice->channelContext[0]->KDFContext = (uint8_t *)malloc(contextAlice->channelContext[0]->KDFContextLength*sizeof(uint8_t));
	memcpy(contextAlice->channelContext[0]->KDFContext, contextAlice->selfZID, 12); /* ZIDi*/
	memcpy(contextAlice->channelContext[0]->KDFContext+12, contextAlice->peerZID, 12); /* ZIDr */
	memcpy(contextAlice->channelContext[0]->KDFContext+24, alice_totalHash, 32); /* total Hash*/

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
	memcpy(dataToHash+hashDataIndex, contextAlice->channelContext[0]->KDFContext, contextAlice->channelContext[0]->KDFContextLength);
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

	contextAlice->channelContext[0]->s0 = (uint8_t *)malloc(32*sizeof(uint8_t));
	contextAlice->channelContext[0]->hashFunction(dataToHash, totalHashDataLength, 32, contextAlice->channelContext[0]->s0);

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
	contextBob->channelContext[0]->KDFContextLength = 24+32;/* actual depends on selected hash length*/
	contextBob->channelContext[0]->KDFContext = (uint8_t *)malloc(contextBob->channelContext[0]->KDFContextLength*sizeof(uint8_t));
	memcpy(contextBob->channelContext[0]->KDFContext, contextBob->peerZID, 12); /* ZIDi*/
	memcpy(contextBob->channelContext[0]->KDFContext+12, contextBob->selfZID, 12); /* ZIDr */
	memcpy(contextBob->channelContext[0]->KDFContext+24, bob_totalHash, 32); /* total Hash*/

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
	memcpy(dataToHash+hashDataIndex, contextBob->channelContext[0]->KDFContext, contextBob->channelContext[0]->KDFContextLength);
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

	contextBob->channelContext[0]->s0 = (uint8_t *)malloc(32*sizeof(uint8_t));
	contextBob->channelContext[0]->hashFunction(dataToHash, totalHashDataLength, 32, contextBob->channelContext[0]->s0);
	

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
	if (memcmp(contextBob->channelContext[0]->s0, contextAlice->channelContext[0]->s0, 32)==0) {
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
	retval = bzrtp_keyDerivationFunction(contextAlice->channelContext[0]->s0, contextAlice->channelContext[0]->hashLength,
		(uint8_t *)"ZRTP Session Key", 16,
		contextAlice->channelContext[0]->KDFContext, contextAlice->channelContext[0]->KDFContextLength, /* this one too depends on selected hash */
		contextAlice->channelContext[0]->hashLength,
		(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[0]->hmacFunction,
		contextAlice->ZRTPSess);

	contextBob->ZRTPSessLength=32; /* must be set to the length of negotiated hash */
	contextBob->ZRTPSess = (uint8_t *)malloc(contextBob->ZRTPSessLength*sizeof(uint8_t));
	retval = bzrtp_keyDerivationFunction(contextBob->channelContext[0]->s0, contextBob->channelContext[0]->hashLength,
		(uint8_t *)"ZRTP Session Key", 16,
		contextBob->channelContext[0]->KDFContext, contextBob->channelContext[0]->KDFContextLength, /* this one too depends on selected hash */
		contextBob->channelContext[0]->hashLength,
		(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[0]->hmacFunction,
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
	retval = bzrtp_keyDerivationFunction(contextAlice->channelContext[0]->s0, contextAlice->channelContext[0]->hashLength,
			(uint8_t *)"SAS", 3,
			contextAlice->channelContext[0]->KDFContext, contextAlice->channelContext[0]->KDFContextLength, /* this one too depends on selected hash */
			256/8, /* function gets L in bytes */
			(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[0]->hmacFunction,
			alice_sasHash);

	uint8_t bob_sasHash[32];
	retval = bzrtp_keyDerivationFunction(contextBob->channelContext[0]->s0, contextBob->channelContext[0]->hashLength,
			(uint8_t *)"SAS", 3,
			contextBob->channelContext[0]->KDFContext, contextBob->channelContext[0]->KDFContextLength, /* this one too depends on selected hash */
			256/8, /* function gets L in bytes */
			(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[0]->hmacFunction,
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
	contextAlice->channelContext[0]->sasFunction(sasValue, sas);

	printf("Alice SAS is %.4s\n", sas);

	sasValue = ((uint32_t)bob_sasHash[0]<<24) | ((uint32_t)bob_sasHash[1]<<16) | ((uint32_t)bob_sasHash[2]<<8) | ((uint32_t)(bob_sasHash[3]));
	contextBob->channelContext[0]->sasFunction(sasValue, sas);
	
	printf("Bob SAS is %.4s\n", sas);


	/* now derive the other keys (mackeyi, mackeyr, zrtpkeyi and zrtpkeyr, srtpkeys and salt) */
	contextAlice->channelContext[0]->mackeyi = (uint8_t *)malloc(contextAlice->channelContext[0]->hashLength*(sizeof(uint8_t)));
	contextAlice->channelContext[0]->mackeyr = (uint8_t *)malloc(contextAlice->channelContext[0]->hashLength*(sizeof(uint8_t)));
	contextAlice->channelContext[0]->zrtpkeyi = (uint8_t *)malloc(contextAlice->channelContext[0]->cipherKeyLength*(sizeof(uint8_t)));
	contextAlice->channelContext[0]->zrtpkeyr = (uint8_t *)malloc(contextAlice->channelContext[0]->cipherKeyLength*(sizeof(uint8_t)));
	contextBob->channelContext[0]->mackeyi = (uint8_t *)malloc(contextBob->channelContext[0]->hashLength*(sizeof(uint8_t)));
	contextBob->channelContext[0]->mackeyr = (uint8_t *)malloc(contextBob->channelContext[0]->hashLength*(sizeof(uint8_t)));
	contextBob->channelContext[0]->zrtpkeyi = (uint8_t *)malloc(contextBob->channelContext[0]->cipherKeyLength*(sizeof(uint8_t)));
	contextBob->channelContext[0]->zrtpkeyr = (uint8_t *)malloc(contextBob->channelContext[0]->cipherKeyLength*(sizeof(uint8_t)));

	/* Alice */
	retval = bzrtp_keyDerivationFunction(contextAlice->channelContext[0]->s0, contextAlice->channelContext[0]->hashLength, (uint8_t *)"Initiator HMAC key", 18, contextAlice->channelContext[0]->KDFContext, contextAlice->channelContext[0]->KDFContextLength, contextAlice->channelContext[0]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[0]->hmacFunction, contextAlice->channelContext[0]->mackeyi);
	retval += bzrtp_keyDerivationFunction(contextAlice->channelContext[0]->s0, contextAlice->channelContext[0]->hashLength, (uint8_t *)"Responder HMAC key", 18, contextAlice->channelContext[0]->KDFContext, contextAlice->channelContext[0]->KDFContextLength, contextAlice->channelContext[0]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[0]->hmacFunction, contextAlice->channelContext[0]->mackeyr);
	retval += bzrtp_keyDerivationFunction(contextAlice->channelContext[0]->s0, contextAlice->channelContext[0]->hashLength, (uint8_t *)"Initiator ZRTP key", 18, contextAlice->channelContext[0]->KDFContext, contextAlice->channelContext[0]->KDFContextLength, contextAlice->channelContext[0]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[0]->hmacFunction, contextAlice->channelContext[0]->zrtpkeyi);
	retval += bzrtp_keyDerivationFunction(contextAlice->channelContext[0]->s0, contextAlice->channelContext[0]->hashLength, (uint8_t *)"Responder ZRTP key", 18, contextAlice->channelContext[0]->KDFContext, contextAlice->channelContext[0]->KDFContextLength, contextAlice->channelContext[0]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[0]->hmacFunction, contextAlice->channelContext[0]->zrtpkeyr);

	/* Bob */
	retval = bzrtp_keyDerivationFunction(contextBob->channelContext[0]->s0, contextBob->channelContext[0]->hashLength, (uint8_t *)"Initiator HMAC key", 18, contextBob->channelContext[0]->KDFContext, contextBob->channelContext[0]->KDFContextLength, contextBob->channelContext[0]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[0]->hmacFunction, contextBob->channelContext[0]->mackeyi);
	retval += bzrtp_keyDerivationFunction(contextBob->channelContext[0]->s0, contextBob->channelContext[0]->hashLength, (uint8_t *)"Responder HMAC key", 18, contextBob->channelContext[0]->KDFContext, contextBob->channelContext[0]->KDFContextLength, contextBob->channelContext[0]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[0]->hmacFunction, contextBob->channelContext[0]->mackeyr);
	retval += bzrtp_keyDerivationFunction(contextBob->channelContext[0]->s0, contextBob->channelContext[0]->hashLength, (uint8_t *)"Initiator ZRTP key", 18, contextBob->channelContext[0]->KDFContext, contextBob->channelContext[0]->KDFContextLength, contextBob->channelContext[0]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[0]->hmacFunction, contextBob->channelContext[0]->zrtpkeyi);
	retval += bzrtp_keyDerivationFunction(contextBob->channelContext[0]->s0, contextBob->channelContext[0]->hashLength, (uint8_t *)"Responder ZRTP key", 18, contextBob->channelContext[0]->KDFContext, contextBob->channelContext[0]->KDFContextLength, contextBob->channelContext[0]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[0]->hmacFunction, contextBob->channelContext[0]->zrtpkeyr);


	/* DEBUG compare keys */
	if ((memcmp(contextAlice->channelContext[0]->mackeyi, contextBob->channelContext[0]->mackeyi, contextAlice->channelContext[0]->hashLength)==0) && (memcmp(contextAlice->channelContext[0]->mackeyr, contextBob->channelContext[0]->mackeyr, contextAlice->channelContext[0]->hashLength)==0) && (memcmp(contextAlice->channelContext[0]->zrtpkeyi, contextBob->channelContext[0]->zrtpkeyi, contextAlice->channelContext[0]->cipherKeyLength)==0) && (memcmp(contextAlice->channelContext[0]->zrtpkeyr, contextBob->channelContext[0]->zrtpkeyr, contextAlice->channelContext[0]->cipherKeyLength)==0)) {
		printf("Got the same keys\n");
		CU_PASS("keys match");
	} else {
		printf("ERROR keys differ\n");
		CU_PASS("Keys mismatch");
	}

	/* now Bob build the CONFIRM1 packet and send it to Alice */
	bzrtpPacket_t *bob_Confirm1 = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[0], MSGTYPE_CONFIRM1, &retval);
	retval += bzrtp_packetBuild(contextBob, contextBob->channelContext[0], bob_Confirm1, contextBob->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextBob->channelContext[0]->selfSequenceNumber++;
	}
	printf("Bob building Confirm1 return %x\n", retval);

	bzrtpPacket_t *alice_Confirm1FromBob = bzrtp_packetCheck(bob_Confirm1->packetString, bob_Confirm1->messageLength+16, contextAlice->channelContext[0]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, contextAlice->channelContext[0], bob_Confirm1->packetString, bob_Confirm1->messageLength+16, alice_Confirm1FromBob);
	printf ("Alice parsing confirm1 returns %x\n", retval);
	bzrtpConfirmMessage_t *alice_Confirm1FromBob_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->channelContext[0]->peerSequenceNumber = alice_Confirm1FromBob->sequenceNumber;
		alice_Confirm1FromBob_message = (bzrtpConfirmMessage_t *)alice_Confirm1FromBob->messageData;
		memcpy(contextAlice->channelContext[0]->peerH[0], alice_Confirm1FromBob_message->H0, 32);
	}

	packetDump(bob_Confirm1,1);
	packetDump(alice_Confirm1FromBob,0);

	/* now Alice build the CONFIRM2 packet and send it to Bob */
	bzrtpPacket_t *alice_Confirm2 = bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[0], MSGTYPE_CONFIRM2, &retval);
	retval += bzrtp_packetBuild(contextAlice, contextAlice->channelContext[0], alice_Confirm2, contextAlice->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->channelContext[0]->selfSequenceNumber++;
	}
	printf("Alice building Confirm2 return %x\n", retval);

	bzrtpPacket_t *bob_Confirm2FromAlice = bzrtp_packetCheck(alice_Confirm2->packetString, alice_Confirm2->messageLength+16, contextBob->channelContext[0]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextBob, contextBob->channelContext[0], alice_Confirm2->packetString, alice_Confirm2->messageLength+16, bob_Confirm2FromAlice);
	printf ("Bob parsing confirm2 returns %x\n", retval);
	bzrtpConfirmMessage_t *bob_Confirm2FromAlice_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextBob->channelContext[0]->peerSequenceNumber = bob_Confirm2FromAlice->sequenceNumber;
		bob_Confirm2FromAlice_message = (bzrtpConfirmMessage_t *)bob_Confirm2FromAlice->messageData;
		memcpy(contextBob->channelContext[0]->peerH[0], bob_Confirm2FromAlice_message->H0, 32);
		/* set bob's status to secure */
		contextBob->isSecure = 1;
	}

	packetDump(alice_Confirm2,1);
	packetDump(bob_Confirm2FromAlice,0);

	/* Bob build the conf2Ack and send it to Alice */
	bzrtpPacket_t *bob_Conf2ACK =  bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[0], MSGTYPE_CONF2ACK, &retval);
	retval += bzrtp_packetBuild(contextBob, contextBob->channelContext[0], bob_Conf2ACK, contextBob->channelContext[0]->selfSequenceNumber);
	if (retval == 0) {
		contextBob->channelContext[0]->selfSequenceNumber++;
	}
	printf("Bob building Conf2ACK return %x\n", retval);

	bzrtpPacket_t *alice_Conf2ACKFromBob = bzrtp_packetCheck(bob_Conf2ACK->packetString, bob_Conf2ACK->messageLength+16, contextAlice->channelContext[0]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, contextAlice->channelContext[0], bob_Conf2ACK->packetString, bob_Conf2ACK->messageLength+16, alice_Conf2ACKFromBob);
	printf ("Alice parsing conf2ACK returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->channelContext[0]->peerSequenceNumber = alice_Conf2ACKFromBob->sequenceNumber;
		/* set Alice's status to secure */
		contextAlice->isSecure = 1;
	}

	dumpContext("Alice", contextAlice);	
	dumpContext("Bob", contextBob);	

	printf("\n\n\n\n\n*************************************************************\n        SECOND CHANNEL\n**********************************************\n\n");
	/* Now create a second channel for Bob and Alice */
	retval = bzrtp_addChannel(contextAlice, 0x45678901);
	printf("Add channel to Alice's context returns %d\n", retval);
	retval = bzrtp_addChannel(contextBob, 0x54321098);
	printf("Add channel to Bob's context returns %d\n", retval);

	/* create hello packets for this channel */
	alice_Hello = bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[1], MSGTYPE_HELLO, &retval);
	if (bzrtp_packetBuild(contextAlice, contextAlice->channelContext[1], alice_Hello, contextAlice->channelContext[1]->selfSequenceNumber) ==0) {
		contextAlice->channelContext[1]->selfSequenceNumber++;
		contextAlice->channelContext[1]->selfPackets[HELLO_MESSAGE_STORE_ID] = alice_Hello;
	}
	bob_Hello = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[1], MSGTYPE_HELLO, &retval);
	if (bzrtp_packetBuild(contextBob, contextBob->channelContext[1], bob_Hello, contextBob->channelContext[1]->selfSequenceNumber) ==0) {
		contextBob->channelContext[1]->selfSequenceNumber++;
		contextBob->channelContext[1]->selfPackets[HELLO_MESSAGE_STORE_ID] = bob_Hello;
	}

	/* now send Alice Hello's to Bob and vice-versa, so they parse them */
	alice_HelloFromBob = bzrtp_packetCheck(bob_Hello->packetString, bob_Hello->messageLength+16, contextAlice->channelContext[1]->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextAlice, contextAlice->channelContext[0], bob_Hello->packetString, bob_Hello->messageLength+16, alice_HelloFromBob);
	printf ("Alice parsing returns %x\n", retval);
	if (retval==0) {
		contextAlice->channelContext[1]->peerSequenceNumber = alice_HelloFromBob->sequenceNumber;
		/* save bob's Hello packet in Alice's context */
		contextAlice->channelContext[1]->peerPackets[HELLO_MESSAGE_STORE_ID] = alice_HelloFromBob;

		/* we are already secured (shall check isSecure==1), so we just need to check that peer Hello have the Mult in his key agreement list of supported algo */
		bzrtpHelloMessage_t *alice_HelloFromBob_message = (bzrtpHelloMessage_t *)alice_HelloFromBob->messageData;
		int i;
		uint8_t checkPeerSupportMultiChannel = 0;
		for (i=0; i<alice_HelloFromBob_message->kc; i++) {
			if (alice_HelloFromBob_message->supportedKeyAgreement[i] == ZRTP_KEYAGREEMENT_Mult) {
				checkPeerSupportMultiChannel = 1;
			}
		}

		/* ok multi channel is supported*/
		if (checkPeerSupportMultiChannel == 1) {
			printf("Alice found that Bob supports multi channel\n");
			/* now set the choosen algos, they MUST be the same than main channel (channel 0) except for keyAgreement which is set to mult */
			contextAlice->channelContext[1]->hashAlgo = contextAlice->channelContext[0]->hashAlgo;
			contextAlice->channelContext[1]->hashLength = contextAlice->channelContext[0]->hashLength;
			contextAlice->channelContext[1]->cipherAlgo = contextAlice->channelContext[0]->cipherAlgo;
			contextAlice->channelContext[1]->cipherKeyLength = contextAlice->channelContext[0]->cipherKeyLength;
			contextAlice->channelContext[1]->authTagAlgo = contextAlice->channelContext[0]->authTagAlgo;
			contextAlice->channelContext[1]->sasAlgo = contextAlice->channelContext[0]->sasAlgo;
			contextAlice->channelContext[1]->keyAgreementAlgo = ZRTP_KEYAGREEMENT_Mult;
			contextAlice->channelContext[1]->keyAgreementLength = 0; /* no public values exchanged in Multi channel mode */

			updateCryptoFunctionPointers(contextAlice->channelContext[1]);
		} else {
			printf("ERROR : Alice found that Bob doesn't support multi channel\n");
		}

	}

	bob_HelloFromAlice = bzrtp_packetCheck(alice_Hello->packetString, alice_Hello->messageLength+16, contextBob->channelContext[1]->peerSequenceNumber, &retval);
	retval +=  bzrtp_packetParser(contextBob, contextBob->channelContext[1], alice_Hello->packetString, alice_Hello->messageLength+16, bob_HelloFromAlice);
	printf ("Bob parsing returns %x\n", retval);
	if (retval==0) {
		contextBob->channelContext[1]->peerSequenceNumber = bob_HelloFromAlice->sequenceNumber;
		/* save alice's Hello packet in bob's context */
		contextBob->channelContext[1]->peerPackets[HELLO_MESSAGE_STORE_ID] = bob_HelloFromAlice;

		/* we are already secured (shall check isSecure==1), so we just need to check that peer Hello have the Mult in his key agreement list of supported algo */
		bzrtpHelloMessage_t *bob_HelloFromAlice_message = (bzrtpHelloMessage_t *)bob_HelloFromAlice->messageData;
		int i;
		uint8_t checkPeerSupportMultiChannel = 0;
		for (i=0; i<bob_HelloFromAlice_message->kc; i++) {
			if (bob_HelloFromAlice_message->supportedKeyAgreement[i] == ZRTP_KEYAGREEMENT_Mult) {
				checkPeerSupportMultiChannel = 1;
			}
		}

		/* ok multi channel is supported*/
		if (checkPeerSupportMultiChannel == 1) {
			printf("Bob found that Alice supports multi channel\n");
			/* now set the choosen algos, they MUST be the same than main channel (channel 0) except for keyAgreement which is set to mult */
			contextBob->channelContext[1]->hashAlgo = contextBob->channelContext[0]->hashAlgo;
			contextBob->channelContext[1]->hashLength = contextBob->channelContext[0]->hashLength;
			contextBob->channelContext[1]->cipherAlgo = contextBob->channelContext[0]->cipherAlgo;
			contextBob->channelContext[1]->cipherKeyLength = contextBob->channelContext[0]->cipherKeyLength;
			contextBob->channelContext[1]->authTagAlgo = contextBob->channelContext[0]->authTagAlgo;
			contextBob->channelContext[1]->sasAlgo = contextBob->channelContext[0]->sasAlgo;
			contextBob->channelContext[1]->keyAgreementAlgo = ZRTP_KEYAGREEMENT_Mult;
			contextBob->channelContext[1]->keyAgreementLength = 0; /* no public values exchanged in Multi channel mode */

			updateCryptoFunctionPointers(contextBob->channelContext[1]);
		} else {
			printf("ERROR : Bob found that Alice doesn't support multi channel\n");
		}
	}

	/* update context with hello message information : H3  and compute initiator and responder's shared secret Hashs */
	alice_HelloFromBob_message = (bzrtpHelloMessage_t *)alice_HelloFromBob->messageData;
	memcpy(contextAlice->channelContext[1]->peerH[3], alice_HelloFromBob_message->H3, 32);
	bob_HelloFromAlice_message = (bzrtpHelloMessage_t *)bob_HelloFromAlice->messageData;
	memcpy(contextBob->channelContext[1]->peerH[3], bob_HelloFromAlice_message->H3, 32);


	/* here we shall exchange Hello ACK but it is just a test and was done already for channel 0, skip it as it is useless for the test */

	/* Bob will be the initiator, so compute a commit for him */
	bob_Commit = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[1], MSGTYPE_COMMIT, &retval);
	retval += bzrtp_packetBuild(contextBob, contextBob->channelContext[1], bob_Commit, contextBob->channelContext[1]->selfSequenceNumber);
	if (retval == 0) {
		contextBob->channelContext[1]->selfSequenceNumber++;
		contextBob->channelContext[1]->selfPackets[COMMIT_MESSAGE_STORE_ID] = bob_Commit;
	}
	printf("Bob building Commit return %x\n", retval);


	/* and send it to Alice */
	alice_CommitFromBob = bzrtp_packetCheck(bob_Commit->packetString, bob_Commit->messageLength+16, contextAlice->channelContext[1]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, contextAlice->channelContext[1], bob_Commit->packetString, bob_Commit->messageLength+16, alice_CommitFromBob);
	printf ("Alice parsing Commit returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->channelContext[1]->peerSequenceNumber = alice_CommitFromBob->sequenceNumber;
		/* Alice will be the initiator (commit contention not implemented in this test) so just discard bob's commit */
		bzrtpCommitMessage_t *alice_CommitFromBob_message = (bzrtpCommitMessage_t *)alice_CommitFromBob->messageData;
		memcpy(contextAlice->channelContext[1]->peerH[2], alice_CommitFromBob_message->H2, 32); 
		contextAlice->channelContext[1]->peerPackets[COMMIT_MESSAGE_STORE_ID] = alice_CommitFromBob;
	}
	packetDump(alice_CommitFromBob, 0);

	/* for test purpose define Alice as the responder */
	contextAlice->channelContext[1]->role = RESPONDER;

	/* compute the total hash as in rfc section 4.4.3.2 total_hash = hash(Hello of responder || Commit) */
	totalHashDataLength = alice_Hello->messageLength + bob_Commit->messageLength;
	dataToHash = (uint8_t *)malloc(totalHashDataLength*sizeof(uint8_t));
	hashDataIndex = 0;

	/* get all data from Alice */
	memcpy(dataToHash, contextAlice->channelContext[1]->selfPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->channelContext[1]->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextAlice->channelContext[1]->selfPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextAlice->channelContext[1]->peerPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextAlice->channelContext[1]->peerPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);
	contextAlice->channelContext[1]->hashFunction(dataToHash, totalHashDataLength, 32, alice_totalHash);

	/* get all data from Bob */
	hashDataIndex = 0;
	memcpy(dataToHash, contextBob->channelContext[1]->peerPackets[HELLO_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->channelContext[1]->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength);
	hashDataIndex += contextBob->channelContext[1]->peerPackets[HELLO_MESSAGE_STORE_ID]->messageLength;
	memcpy(dataToHash+hashDataIndex, contextBob->channelContext[1]->selfPackets[COMMIT_MESSAGE_STORE_ID]->packetString+ZRTP_PACKET_HEADER_LENGTH, contextBob->channelContext[1]->selfPackets[COMMIT_MESSAGE_STORE_ID]->messageLength);

	contextBob->channelContext[1]->hashFunction(dataToHash, totalHashDataLength, 32, bob_totalHash);
	if (memcmp(bob_totalHash, alice_totalHash, 32) == 0) {
		printf("Got the same total hash\n");
		CU_PASS("Total Hash match");
	} else {
		printf("AARGG!! total hash mismatch");
		CU_FAIL("Total Hash mismatch");
	}

	free(dataToHash);


	/* compute the KDF Context as in rfc section 4.4.3.2 KDF_Context = (ZIDi || ZIDr || total_hash) */
	contextAlice->channelContext[1]->KDFContextLength = 24 + contextAlice->channelContext[1]->hashLength;
	contextAlice->channelContext[1]->KDFContext = (uint8_t *)malloc(contextAlice->channelContext[1]->KDFContextLength*sizeof(uint8_t));
	memcpy(contextAlice->channelContext[1]->KDFContext, contextAlice->peerZID, 12);
	memcpy(contextAlice->channelContext[1]->KDFContext+12, contextAlice->selfZID, 12);
	memcpy(contextAlice->channelContext[1]->KDFContext+24, alice_totalHash, contextAlice->channelContext[1]->hashLength);

	contextBob->channelContext[1]->KDFContextLength = 24 + contextBob->channelContext[1]->hashLength;
	contextBob->channelContext[1]->KDFContext = (uint8_t *)malloc(contextBob->channelContext[1]->KDFContextLength*sizeof(uint8_t));
	memcpy(contextBob->channelContext[1]->KDFContext, contextBob->selfZID, 12);
	memcpy(contextBob->channelContext[1]->KDFContext+12, contextBob->peerZID, 12);
	memcpy(contextBob->channelContext[1]->KDFContext+24, bob_totalHash, contextBob->channelContext[1]->hashLength);

	if (memcmp(contextBob->channelContext[1]->KDFContext, contextAlice->channelContext[1]->KDFContext, 56) == 0) {
		printf("Got the same total KDF Context\n");
		CU_PASS("KDFContext match");
	} else {
		printf("AARGG!! KDF Context mismatch");
		CU_FAIL("KDF Context mismatch");
	}

	/* compute s0 as in rfc section 4.4.3.2  s0 = KDF(ZRTPSess, "ZRTP MSK", KDF_Context, negotiated hash length) */
	contextBob->channelContext[1]->s0 = (uint8_t *)malloc(contextBob->channelContext[1]->hashLength*sizeof(uint8_t));
	contextAlice->channelContext[1]->s0 = (uint8_t *)malloc(contextAlice->channelContext[1]->hashLength*sizeof(uint8_t));
	retval = bzrtp_keyDerivationFunction(contextBob->ZRTPSess, contextBob->ZRTPSessLength,
		(uint8_t *)"ZRTP MSK", 8,
		contextBob->channelContext[1]->KDFContext, contextBob->channelContext[1]->KDFContextLength, /* this one too depends on selected hash */
		contextBob->channelContext[1]->hashLength,
		(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[1]->hmacFunction,
		contextBob->channelContext[1]->s0);

	retval = bzrtp_keyDerivationFunction(contextAlice->ZRTPSess, contextAlice->ZRTPSessLength,
		(uint8_t *)"ZRTP MSK", 8,
		contextAlice->channelContext[1]->KDFContext, contextAlice->channelContext[1]->KDFContextLength, /* this one too depends on selected hash */
		contextAlice->channelContext[1]->hashLength,
		(void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[1]->hmacFunction,
		contextAlice->channelContext[1]->s0);

	if (memcmp(contextBob->channelContext[1]->s0, contextAlice->channelContext[1]->s0, contextAlice->channelContext[1]->hashLength) == 0) {
		printf("Got the same s0\n");
		CU_PASS("s0 match");
	} else {
		printf("AARGG!! s0 mismatch");
		CU_FAIL("s0 mismatch");
	}


	/* the rest of key derivation is common to DH mode, no need to test it as it has been done before for channel 0 */
	/* we must anyway derive zrtp and mac key for initiator and responder in order to be able to build the confirm packets */
	contextAlice->channelContext[1]->mackeyi = (uint8_t *)malloc(contextAlice->channelContext[1]->hashLength*(sizeof(uint8_t)));
	contextAlice->channelContext[1]->mackeyr = (uint8_t *)malloc(contextAlice->channelContext[1]->hashLength*(sizeof(uint8_t)));
	contextAlice->channelContext[1]->zrtpkeyi = (uint8_t *)malloc(contextAlice->channelContext[1]->cipherKeyLength*(sizeof(uint8_t)));
	contextAlice->channelContext[1]->zrtpkeyr = (uint8_t *)malloc(contextAlice->channelContext[1]->cipherKeyLength*(sizeof(uint8_t)));
	contextBob->channelContext[1]->mackeyi = (uint8_t *)malloc(contextBob->channelContext[1]->hashLength*(sizeof(uint8_t)));
	contextBob->channelContext[1]->mackeyr = (uint8_t *)malloc(contextBob->channelContext[1]->hashLength*(sizeof(uint8_t)));
	contextBob->channelContext[1]->zrtpkeyi = (uint8_t *)malloc(contextBob->channelContext[1]->cipherKeyLength*(sizeof(uint8_t)));
	contextBob->channelContext[1]->zrtpkeyr = (uint8_t *)malloc(contextBob->channelContext[1]->cipherKeyLength*(sizeof(uint8_t)));

	/* Alice */
	retval = bzrtp_keyDerivationFunction(contextAlice->channelContext[1]->s0, contextAlice->channelContext[1]->hashLength, (uint8_t *)"Initiator HMAC key", 18, contextAlice->channelContext[1]->KDFContext, contextAlice->channelContext[1]->KDFContextLength, contextAlice->channelContext[1]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[1]->hmacFunction, contextAlice->channelContext[1]->mackeyi);
	retval += bzrtp_keyDerivationFunction(contextAlice->channelContext[1]->s0, contextAlice->channelContext[1]->hashLength, (uint8_t *)"Responder HMAC key", 18, contextAlice->channelContext[1]->KDFContext, contextAlice->channelContext[1]->KDFContextLength, contextAlice->channelContext[1]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[1]->hmacFunction, contextAlice->channelContext[1]->mackeyr);
	retval += bzrtp_keyDerivationFunction(contextAlice->channelContext[1]->s0, contextAlice->channelContext[1]->hashLength, (uint8_t *)"Initiator ZRTP key", 18, contextAlice->channelContext[1]->KDFContext, contextAlice->channelContext[1]->KDFContextLength, contextAlice->channelContext[1]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[1]->hmacFunction, contextAlice->channelContext[1]->zrtpkeyi);
	retval += bzrtp_keyDerivationFunction(contextAlice->channelContext[1]->s0, contextAlice->channelContext[1]->hashLength, (uint8_t *)"Responder ZRTP key", 18, contextAlice->channelContext[1]->KDFContext, contextAlice->channelContext[1]->KDFContextLength, contextAlice->channelContext[1]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextAlice->channelContext[1]->hmacFunction, contextAlice->channelContext[1]->zrtpkeyr);

	/* Bob */
	retval = bzrtp_keyDerivationFunction(contextBob->channelContext[1]->s0, contextBob->channelContext[1]->hashLength, (uint8_t *)"Initiator HMAC key", 18, contextBob->channelContext[1]->KDFContext, contextBob->channelContext[1]->KDFContextLength, contextBob->channelContext[1]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[1]->hmacFunction, contextBob->channelContext[1]->mackeyi);
	retval += bzrtp_keyDerivationFunction(contextBob->channelContext[1]->s0, contextBob->channelContext[1]->hashLength, (uint8_t *)"Responder HMAC key", 18, contextBob->channelContext[1]->KDFContext, contextBob->channelContext[1]->KDFContextLength, contextBob->channelContext[1]->hashLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[1]->hmacFunction, contextBob->channelContext[1]->mackeyr);
	retval += bzrtp_keyDerivationFunction(contextBob->channelContext[1]->s0, contextBob->channelContext[1]->hashLength, (uint8_t *)"Initiator ZRTP key", 18, contextBob->channelContext[1]->KDFContext, contextBob->channelContext[1]->KDFContextLength, contextBob->channelContext[1]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[1]->hmacFunction, contextBob->channelContext[1]->zrtpkeyi);
	retval += bzrtp_keyDerivationFunction(contextBob->channelContext[1]->s0, contextBob->channelContext[1]->hashLength, (uint8_t *)"Responder ZRTP key", 18, contextBob->channelContext[1]->KDFContext, contextBob->channelContext[1]->KDFContextLength, contextBob->channelContext[1]->cipherKeyLength, (void (*)(uint8_t *, uint8_t,  uint8_t *, uint32_t,  uint8_t,  uint8_t *))contextBob->channelContext[1]->hmacFunction, contextBob->channelContext[1]->zrtpkeyr);

	/* DEBUG compare keys */
	if ((memcmp(contextAlice->channelContext[1]->mackeyi, contextBob->channelContext[1]->mackeyi, contextAlice->channelContext[1]->hashLength)==0) && (memcmp(contextAlice->channelContext[1]->mackeyr, contextBob->channelContext[1]->mackeyr, contextAlice->channelContext[1]->hashLength)==0) && (memcmp(contextAlice->channelContext[1]->zrtpkeyi, contextBob->channelContext[1]->zrtpkeyi, contextAlice->channelContext[1]->cipherKeyLength)==0) && (memcmp(contextAlice->channelContext[1]->zrtpkeyr, contextBob->channelContext[1]->zrtpkeyr, contextAlice->channelContext[1]->cipherKeyLength)==0)) {
		printf("Got the same keys\n");
		CU_PASS("keys match");
	} else {
		printf("ERROR keys differ\n");
		CU_PASS("Keys mismatch");
	}

	/* now Alice build a confirm1 packet */
	bzrtpPacket_t *alice_Confirm1 = bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[1], MSGTYPE_CONFIRM1, &retval);
	retval += bzrtp_packetBuild(contextAlice, contextAlice->channelContext[1], alice_Confirm1, contextAlice->channelContext[1]->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->channelContext[1]->selfSequenceNumber++;
	}
	printf("Alice building Confirm1 return %x\n", retval);

	bzrtpPacket_t *bob_Confirm1FromAlice = bzrtp_packetCheck(alice_Confirm1->packetString, alice_Confirm1->messageLength+16, contextBob->channelContext[1]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextBob, contextBob->channelContext[1], alice_Confirm1->packetString, alice_Confirm1->messageLength+16, bob_Confirm1FromAlice);
	printf ("Bob parsing confirm1 returns %x\n", retval);
	bzrtpConfirmMessage_t *bob_Confirm1FromAlice_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextBob->channelContext[1]->peerSequenceNumber = bob_Confirm1FromAlice->sequenceNumber;
		bob_Confirm1FromAlice_message = (bzrtpConfirmMessage_t *)bob_Confirm1FromAlice->messageData;
		memcpy(contextBob->channelContext[1]->peerH[0], bob_Confirm1FromAlice_message->H0, 32);
	}

	packetDump(bob_Confirm1FromAlice,0);

	/* now Bob build the CONFIRM2 packet and send it to Alice */
	bzrtpPacket_t *bob_Confirm2 = bzrtp_createZrtpPacket(contextBob, contextBob->channelContext[1], MSGTYPE_CONFIRM2, &retval);
	retval += bzrtp_packetBuild(contextBob, contextBob->channelContext[1], bob_Confirm2, contextBob->channelContext[1]->selfSequenceNumber);
	if (retval == 0) {
		contextBob->channelContext[1]->selfSequenceNumber++;
	}
	printf("Bob building Confirm2 return %x\n", retval);
	bzrtpPacket_t *alice_Confirm2FromBob = bzrtp_packetCheck(bob_Confirm2->packetString, bob_Confirm2->messageLength+16, contextAlice->channelContext[1]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextAlice, contextAlice->channelContext[1], bob_Confirm2->packetString, bob_Confirm2->messageLength+16, alice_Confirm2FromBob);
	printf ("Alice parsing confirm2 returns %x\n", retval);
	bzrtpConfirmMessage_t *alice_Confirm2FromBob_message=NULL;
	if (retval==0) {
		/* update context with the information found in the packet */
		contextAlice->channelContext[1]->peerSequenceNumber = alice_Confirm2FromBob->sequenceNumber;
		alice_Confirm2FromBob_message = (bzrtpConfirmMessage_t *)alice_Confirm2FromBob->messageData;
		memcpy(contextAlice->channelContext[1]->peerH[0], alice_Confirm2FromBob_message->H0, 32);
	}

	packetDump(alice_Confirm2FromBob,0);

	/* Alice build the conf2Ack and send it to Bob */
	bzrtpPacket_t *alice_Conf2ACK =  bzrtp_createZrtpPacket(contextAlice, contextAlice->channelContext[1], MSGTYPE_CONF2ACK, &retval);
	retval += bzrtp_packetBuild(contextAlice, contextAlice->channelContext[1], alice_Conf2ACK, contextAlice->channelContext[1]->selfSequenceNumber);
	if (retval == 0) {
		contextAlice->channelContext[1]->selfSequenceNumber++;
	}
	printf("Alice building Conf2ACK return %x\n", retval);

	bzrtpPacket_t *bob_Conf2ACKFromAlice = bzrtp_packetCheck(alice_Conf2ACK->packetString, alice_Conf2ACK->messageLength+16, contextBob->channelContext[1]->peerSequenceNumber, &retval);
	retval += bzrtp_packetParser(contextBob, contextBob->channelContext[1], alice_Conf2ACK->packetString, alice_Conf2ACK->messageLength+16, bob_Conf2ACKFromAlice);
	printf ("Bob parsing conf2ACK returns %x\n", retval);
	if (retval==0) {
		/* update context with the information found in the packet */
		contextBob->channelContext[1]->peerSequenceNumber = bob_Conf2ACKFromAlice->sequenceNumber;
	}






/*
	dumpContext("\nAlice", contextAlice);
	dumpContext("\nBob", contextBob);
*/
	/* destroy the context */
	bzrtp_destroyBzrtpContext(contextBob, 0x87654321);
	bzrtp_destroyBzrtpContext(contextAlice, 0x12345678);

}


typedef struct packetDatas_struct {
	uint8_t packetString[1000];
	uint16_t packetLength;
} packetDatas_t;

/* Alice and Bob packet queues are globals */
packetDatas_t aliceQueue[10];
packetDatas_t bobQueue[10];
uint8_t aliceQueueIndex = 0;
uint8_t bobQueueIndex = 0;

uint8_t block_Hello = 0;

/* this is a callback function for send data, just dump the packet */
/* client Data is a my_Context_t structure */
int bzrtp_sendData(void *clientData, uint8_t *packetString, uint16_t packetLength) {
	/* get the client Data */
	my_Context_t *contexts = (my_Context_t *)clientData;

	printf ("%s sends a message!\n", contexts->nom);
	int retval;
	bzrtpPacket_t *zrtpPacket = bzrtp_packetCheck(packetString, packetLength, contexts->peerChannelContext->peerSequenceNumber, &retval);
	if (retval==0) {
		retval = bzrtp_packetParser(contexts->peerContext, contexts->peerChannelContext, packetString, packetLength, zrtpPacket);
		if (retval == 0) {
			packetDump(zrtpPacket,0);
			printHex("Data", packetString, packetLength);
		} else {
			printf("Parse says %04x\n", retval);
		}
	} else {
		printf("Check says %04x\n", retval);
	}

	/* put the message in the message queue */
	if (contexts->nom[0] == 'A') { /* message sent by Alice, put it in Bob's queue */
		/* block the first Hello to force going through wait for hello state and check it is retransmitted */
		if (block_Hello == 0 && zrtpPacket->messageType == MSGTYPE_HELLO) {
			block_Hello = 1;
		} else {
			memcpy(bobQueue[bobQueueIndex].packetString, packetString, packetLength);
			bobQueue[bobQueueIndex++].packetLength = packetLength;
		}
	} else {
		memcpy(aliceQueue[aliceQueueIndex].packetString, packetString, packetLength);
		aliceQueue[aliceQueueIndex++].packetLength = packetLength;
	}

	return 0;
}

uint64_t getCurrentTimeInMs() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec*1000+tv.tv_usec/1000);
}

/* Ping message length is 24 bytes (already define in packetParser.c out of this scope) */
#define ZRTP_PINGMESSAGE_FIXED_LENGTH 24

void test_stateMachine() {
	struct timespec tenMs;
	tenMs.tv_sec = 0;
	tenMs.tv_nsec = 10000000; /* 10 ms */

	/* Create zrtp Context */
	bzrtpContext_t *contextAlice = bzrtp_createBzrtpContext(0x12345678); /* Alice's SSRC of main channel is 12345678 */
	bzrtpContext_t *contextBob = bzrtp_createBzrtpContext(0x87654321); /* Bob's SSRC of main channel is 87654321 */

	/* set the cache related callback functions */
	bzrtp_setCallback(contextAlice, (int (*)())floadAlice, ZRTP_CALLBACK_LOADCACHE);
	bzrtp_setCallback(contextAlice, (int (*)())fwriteAlice, ZRTP_CALLBACK_WRITECACHE);

	bzrtp_setCallback(contextBob, (int (*)())floadBob, ZRTP_CALLBACK_LOADCACHE);
	bzrtp_setCallback(contextBob, (int (*)())fwriteBob, ZRTP_CALLBACK_WRITECACHE);

	/* define the sendData function */
	int retval;
	retval = bzrtp_setCallback(contextAlice, (int (*)())bzrtp_sendData, ZRTP_CALLBACK_SENDDATA);
	retval += bzrtp_setCallback(contextBob, (int (*)())bzrtp_sendData, ZRTP_CALLBACK_SENDDATA);
	printf("Set callbacks return %x\n", retval);

	/* create the client Data and associate them to the channel contexts */
	my_Context_t aliceClientData, bobClientData;
	
	memcpy(aliceClientData.nom, "Alice", 6);
	memcpy(bobClientData.nom, "Bob", 4);
	aliceClientData.peerContext = contextBob;
	aliceClientData.peerChannelContext = contextBob->channelContext[0];
	bobClientData.peerContext = contextAlice;
	bobClientData.peerChannelContext = contextAlice->channelContext[0];
	memcpy(aliceClientData.zidFilename, "test/ZIDAlice.txt", 18);
	memcpy(bobClientData.zidFilename, "test/ZIDBob.txt", 16);


	retval = bzrtp_setClientData(contextAlice, 0x12345678, (void *)&aliceClientData);
	retval += bzrtp_setClientData(contextBob, 0x87654321, (void *)&bobClientData);
	printf("Set client data return %x\n", retval);

	/* run the init */
	bzrtp_initBzrtpContext(contextAlice);
	bzrtp_initBzrtpContext(contextBob);

	/* now start the engine */
	uint64_t initialTime = getCurrentTimeInMs();
	retval = bzrtp_startChannelEngine(contextAlice, 0x12345678);
	printf ("Alice starts return %x\n", retval);
	retval = bzrtp_startChannelEngine(contextBob, 0x87654321);
	printf ("Bob starts return %x\n", retval);

	/* now start infinite loop until we reach secure state */
	while ((contextAlice->isSecure == 0 || contextBob->isSecure == 0) && (getCurrentTimeInMs()-initialTime<5000)){
		int i;
		/* first check the message queue */
		for (i=0; i<aliceQueueIndex; i++) {
			printf("Process a message for Alice\n");
			retval = bzrtp_processMessage(contextAlice, 0x12345678, aliceQueue[i].packetString, aliceQueue[i].packetLength);
			printf("Alice processed message and return %04x\n\n", retval);
			memset(aliceQueue[i].packetString, 0, 1000); /* destroy the packet after sending it to the ZRTP engine */
		}
		aliceQueueIndex = 0;

		for (i=0; i<bobQueueIndex; i++) {
			printf("Process a message for Bob\n");
			retval = bzrtp_processMessage(contextBob, 0x87654321, bobQueue[i].packetString, bobQueue[i].packetLength);
			printf("Bob processed message and return %04x\n\n", retval);
			memset(bobQueue[i].packetString, 0, 1000); /* destroy the packet after sending it to the ZRTP engine */
		}
		bobQueueIndex = 0;


		/* send the actual time to the zrtpContext */
		bzrtp_iterate(contextAlice, 0x12345678, getCurrentTimeInMs());
		bzrtp_iterate(contextBob, 0x87654321, getCurrentTimeInMs());


		/* sleep for 10 ms */
		nanosleep(&tenMs, NULL);
	}

	/* compare SAS and check we are in secure mode */
	CU_ASSERT_TRUE((memcmp(contextAlice->channelContext[0]->srtpSecrets.sas, contextBob->channelContext[0]->srtpSecrets.sas, 4) == 0) && (contextAlice->isSecure == 1) && (contextBob->isSecure == 1));
	
	/*** Send alice a ping message from Bob ***/
	uint8_t pingPacketString[ZRTP_PACKET_OVERHEAD+ZRTP_PINGMESSAGE_FIXED_LENGTH]; /* there is no builder for ping packet and it is 24 bytes long(12 bytes of message header, 12 of data + packet overhead*/

	/* set packet header and CRC */
	/* preambule */
	pingPacketString[0] = 0x10;
	pingPacketString[1] = 0x00;
	/* Sequence number */
	pingPacketString[2] = (uint8_t)((contextBob->channelContext[0]->selfSequenceNumber>>8)&0x00FF);
	pingPacketString[3] = (uint8_t)(contextBob->channelContext[0]->selfSequenceNumber&0x00FF);
	/* ZRTP magic cookie */
	pingPacketString[4] = (uint8_t)((ZRTP_MAGIC_COOKIE>>24)&0xFF);
	pingPacketString[5] = (uint8_t)((ZRTP_MAGIC_COOKIE>>16)&0xFF);
	pingPacketString[6] = (uint8_t)((ZRTP_MAGIC_COOKIE>>8)&0xFF);
	pingPacketString[7] = (uint8_t)(ZRTP_MAGIC_COOKIE&0xFF);
	/* Source Identifier : insert bob's one: 0x87654321 */
	pingPacketString[8] = 0x87;
	pingPacketString[9] = 0x65;
	pingPacketString[10] = 0x43;
	pingPacketString[11] = 0x21;

	/* message header */
	pingPacketString[12] = 0x50;
	pingPacketString[13] = 0x5a;

	/* length in 32 bits words */
	pingPacketString[14] = 0x00;
	pingPacketString[15] = 0x06;

	/* message type "Ping    " */
	memcpy(pingPacketString+16, "Ping    ",8);

	/* Version on 4 bytes is "1.10" */
	memcpy(pingPacketString+24, "1.10", 4);

	/* a endPointHash, use the first 8 bytes of Bob's ZID */
	memcpy(pingPacketString+28, contextBob->selfZID, 8);

	/* CRC */
	uint32_t CRC = bzrtp_CRC32(pingPacketString, ZRTP_PINGMESSAGE_FIXED_LENGTH+ZRTP_PACKET_HEADER_LENGTH);
	uint8_t *CRCbuffer = pingPacketString+ZRTP_PINGMESSAGE_FIXED_LENGTH+ZRTP_PACKET_HEADER_LENGTH;
	*CRCbuffer = (uint8_t)((CRC>>24)&0xFF);
	CRCbuffer++;
	*CRCbuffer = (uint8_t)((CRC>>16)&0xFF);
	CRCbuffer++;
	*CRCbuffer = (uint8_t)((CRC>>8)&0xFF);
	CRCbuffer++;
	*CRCbuffer = (uint8_t)(CRC&0xFF);

	printf("Process a PING message for Alice\n");
	retval = bzrtp_processMessage(contextAlice, 0x12345678, pingPacketString, ZRTP_PACKET_OVERHEAD+ZRTP_PINGMESSAGE_FIXED_LENGTH);
	printf("Alice processed PING message and return %04x\n\n", retval);


	/*** now add a second channel ***/
	retval = bzrtp_addChannel(contextAlice, 0x34567890);
	printf("Add a channel to Alice context, return %x\n", retval);
	retval = bzrtp_addChannel(contextBob, 0x09876543);
	printf("Add a channel to Bob context, return %x\n", retval);

	/* create the client Data and associate them to the channel contexts */
	my_Context_t aliceSecondChannelClientData, bobSecondChannelClientData;
	
	memcpy(aliceSecondChannelClientData.nom, "Alice", 6);
	memcpy(bobSecondChannelClientData.nom, "Bob", 4);
	aliceSecondChannelClientData.peerContext = contextBob;
	aliceSecondChannelClientData.peerChannelContext = contextBob->channelContext[1];
	bobSecondChannelClientData.peerContext = contextAlice;
	bobSecondChannelClientData.peerChannelContext = contextAlice->channelContext[1];

	retval = bzrtp_setClientData(contextAlice, 0x34567890, (void *)&aliceSecondChannelClientData);
	retval += bzrtp_setClientData(contextBob, 0x09876543, (void *)&bobSecondChannelClientData);
	printf("Set client data return %x\n", retval);

	/* start the channels */
	retval = bzrtp_startChannelEngine(contextAlice, 0x34567890);
	printf ("Alice starts return %x\n", retval);
	retval = bzrtp_startChannelEngine(contextBob, 0x09876543);
	printf ("Bob starts return %x\n", retval);

	/* now start infinite loop until we reach secure state */
	while ((getCurrentTimeInMs()-initialTime<2000)){
		int i;
		/* first check the message queue */
		for (i=0; i<aliceQueueIndex; i++) {
			printf("Process a message for Alice\n");
			retval = bzrtp_processMessage(contextAlice, 0x34567890, aliceQueue[i].packetString, aliceQueue[i].packetLength);
			printf("Alice processed message and return %04x\n\n", retval);
			memset(aliceQueue[i].packetString, 0, 1000); /* destroy the packet after sending it to the ZRTP engine */
		}
		aliceQueueIndex = 0;

		for (i=0; i<bobQueueIndex; i++) {
			printf("Process a message for Bob\n");
			retval = bzrtp_processMessage(contextBob, 0x09876543, bobQueue[i].packetString, bobQueue[i].packetLength);
			printf("Bob processed message and return %04x\n\n", retval);
			memset(bobQueue[i].packetString, 0, 1000); /* destroy the packet after sending it to the ZRTP engine */
		}
		bobQueueIndex = 0;


		/* send the actual time to the zrtpContext */
		bzrtp_iterate(contextAlice, 0x34567890, getCurrentTimeInMs());
		bzrtp_iterate(contextBob, 0x09876543, getCurrentTimeInMs());


		/* sleep for 10 ms */
		nanosleep(&tenMs, NULL);
	}



	dumpContext("\nAlice", contextAlice);
	dumpContext("\nBob", contextBob);




}
