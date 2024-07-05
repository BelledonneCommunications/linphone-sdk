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
#include <string.h>
#include <list>

#include "cryptoUtils.h"
#include "bctoolbox/crypto.hh"
#ifdef HAVE_BCTBXPQ
#include "postquantumcryptoengine/crypto.hh"
#endif /* HAVE_BCTBXPQ */

static uint32_t bzrtp_key_agreement_algo_list(void) {
	uint32_t ret = bctbx_key_agreement_algo_list();
#ifdef HAVE_BCTBXPQ
	ret |= bctbxpq_key_agreement_algo_list();
#endif /* HAVE_BCTBXPQ */
	return ret;
}

uint8_t bzrtpUtils_getAllAvailableCryptoTypes(uint8_t algoType, uint8_t availableTypes[256]) {

	switch(algoType) {
	case ZRTP_HASH_TYPE:
		availableTypes[0] = ZRTP_HASH_S256;
		availableTypes[1] = ZRTP_HASH_S384;
		availableTypes[2] = ZRTP_HASH_S512;
		return 3;
	case ZRTP_CIPHERBLOCK_TYPE:
		availableTypes[0] = ZRTP_CIPHER_AES1;
		availableTypes[1] = ZRTP_CIPHER_AES3;
		return 2;
	case ZRTP_AUTHTAG_TYPE:
		availableTypes[0] = ZRTP_AUTHTAG_GCM;
		availableTypes[1] = ZRTP_AUTHTAG_HS32;
		availableTypes[2] = ZRTP_AUTHTAG_HS80;
		return 3;
	case ZRTP_KEYAGREEMENT_TYPE:
	{
		/* get availables types from bctoolbox */
		uint32_t available_key_agreements = bzrtp_key_agreement_algo_list();

		/* Put them in a list in a prefered order, if no configuration is done the first 6 are used */
		uint8_t index=0;
		if (available_key_agreements&BCTBX_ECDH_X25519) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_X255;
			index++;
		}

		if (available_key_agreements&BCTBX_ECDH_X448) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_X448;
			index++;
		}

		/* DH3k is mandatory*/
		availableTypes[index] = ZRTP_KEYAGREEMENT_DH3k;
		index++;

		if (available_key_agreements&BCTBX_KEM_KYBER512) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_KYB1;
			index++;
		}

		if (available_key_agreements&BCTBX_KEM_HQC128) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_HQC1;
			index++;
		}

		if (available_key_agreements&BCTBX_KEM_KYBER768) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_KYB2;
			index++;
		}

		if (available_key_agreements&BCTBX_KEM_HQC192) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_HQC2;
			index++;
		}

		if (available_key_agreements&BCTBX_KEM_KYBER1024) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_KYB3;
			index++;
		}

		if (available_key_agreements&BCTBX_KEM_HQC256) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_HQC3;
			index++;
		}

		if (available_key_agreements&BCTBX_DHM_2048) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_DH2k;
			index++;
		}

		if (available_key_agreements&BCTBX_KEM_X25519) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K255;
			index++;
		}

		if (available_key_agreements&BCTBX_KEM_X448) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K448;
			index++;
		}

		if ((available_key_agreements&BCTBX_KEM_X25519) && (available_key_agreements&BCTBX_KEM_KYBER512)) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K255_KYB512;
			index++;
		}

		if ((available_key_agreements&BCTBX_KEM_X25519) && (available_key_agreements&BCTBX_KEM_HQC128)) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K255_HQC128;
			index++;
		}

		if ((available_key_agreements&BCTBX_KEM_X448) && (available_key_agreements&BCTBX_KEM_KYBER1024)) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K448_KYB1024;
			index++;
		}

		if ((available_key_agreements&BCTBX_KEM_X448) && (available_key_agreements&BCTBX_KEM_HQC256)) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K448_HQC256;
			index++;
		}

		if ((available_key_agreements&BCTBX_KEM_X25519) && (available_key_agreements&BCTBX_KEM_KYBER512) && (available_key_agreements&BCTBX_KEM_HQC128)) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K255_KYB512_HQC128;
			index++;
		}

		if ((available_key_agreements&BCTBX_KEM_X448) && (available_key_agreements&BCTBX_KEM_KYBER1024) && (available_key_agreements&BCTBX_KEM_HQC256)) {
			availableTypes[index] = ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256;
			index++;
		}

		/* Mult is not mandatory but we need it to support old buggy implementation */
		availableTypes[index] = ZRTP_KEYAGREEMENT_Mult;
		return index+1;
	}
	case ZRTP_SAS_TYPE: /* the SAS function is implemented in cryptoUtils.c and then is not directly linked to the polarSSL crypto wrapper */
		availableTypes[0] = ZRTP_SAS_B32;
		availableTypes[1] = ZRTP_SAS_B256;
		return 2;
	default:
		return 0;
	}
}
uint8_t bzrtp_available_key_agreement(uint8_t algos[256]) {
	return bzrtpUtils_getAllAvailableCryptoTypes(ZRTP_KEYAGREEMENT_TYPE, algos);
}

bool_t bzrtp_is_PQ_available(void) {
#ifdef HAVE_BCTBXPQ
	return TRUE;
#else // HAVE_BCTBXPQ
	return FALSE;
#endif // HAVE_BCTBXPQ
}

uint8_t bzrtpUtils_getAvailableCryptoTypes(uint8_t algoType, uint8_t availableTypes[7]) {
	uint8_t allTypes[256];
	memset(allTypes, ZRTP_UNSET_ALGO, 7);
	uint8_t ret = bzrtpUtils_getAllAvailableCryptoTypes(algoType, allTypes);
	memcpy(availableTypes, allTypes, 7);
	if (algoType != ZRTP_KEYAGREEMENT_TYPE) {
		return ret;
	} else {
		memcpy(availableTypes, allTypes, 7);
		if (ret<7) {
			return ret;
		} else {
			/* buggy behavior, we need this one for retrocompatibility, enfore its prensence at the end of the list */
			availableTypes[6] = ZRTP_KEYAGREEMENT_Mult;
			return 7;
		}
	}
}

/** Return mandatory crypto functions. For now we have
 *
 * - Hash: HMAC-SHA256
 * - CipherBlock: AES128
 * - Auth Tag: HMAC-SHA132 and HMAC-SHA180
 * - Key Agreement: DHM3k
 * - Sas: base32
 */
uint8_t bzrtpUtils_getMandatoryCryptoTypes(uint8_t algoType, uint8_t mandatoryTypes[7]) {

	switch(algoType) {
	case ZRTP_HASH_TYPE:
		mandatoryTypes[0] = ZRTP_HASH_S256;
		return 1;
	case ZRTP_CIPHERBLOCK_TYPE:
		mandatoryTypes[0] = ZRTP_CIPHER_AES1;
		return 1;
	case ZRTP_AUTHTAG_TYPE:
		mandatoryTypes[0] = ZRTP_AUTHTAG_HS32;
		mandatoryTypes[1] = ZRTP_AUTHTAG_HS80;
		return 2;
	case ZRTP_KEYAGREEMENT_TYPE:
		mandatoryTypes[0] = ZRTP_KEYAGREEMENT_DH3k;
		mandatoryTypes[1] = ZRTP_KEYAGREEMENT_Mult; /* we must add this one if we want to be able to make multistream */
		return 2;
	case ZRTP_SAS_TYPE:
		mandatoryTypes[0] = ZRTP_SAS_B32;
		return 1;
	default:
		return 0;
	}
}

int bzrtp_keyDerivationFunction(const uint8_t *key, const size_t keyLength,
								const uint8_t *label, const size_t labelLength,
								const uint8_t *context, const size_t contextLength,
								const uint8_t hmacLength,
								void (*hmacFunction)(const uint8_t *, size_t, const uint8_t *, size_t, uint8_t, uint8_t *),
								uint8_t *output) {

	/* get the total length (in bytes) of the data to be hashed */
	/* need to add 4 bytes for the initial constant 0x00000001, 1 byte for the 0x00 separator and 4 bytes for the hmacLength length */
	size_t inputLength = 4 + labelLength + 1 + contextLength + 4;

	/* create the hmac function input */
	uint8_t *input = (uint8_t *)malloc(inputLength*sizeof(uint8_t));

	/* fill the input starting by the 32-bits big-endian interger set to 0x00000001 */
	size_t index = 0;
	input[index++] = 0x00;
	input[index++] = 0x00;
	input[index++] = 0x00;
	input[index++] = 0x01;

	/* concat the label */
	memcpy(input+index, label, labelLength);
	index += labelLength;

	/* a separation byte to 0x00 */
	input[index++] = 0x00;

	/* concat the context string */
	memcpy(input+index, context, contextLength);
	index += contextLength;

	/* end by L(hmacLength) in big-endian. hmacLength is in bytes and must be converted to bits before insertion in the text to hash */
	input[index++] = 0; //(uint8_t)((hmacLength>>21)&0xFF); - hmacLength in bit cannot bit more than 2^11 so the first 2 bytes are always 0
	input[index++] = 0; //(uint8_t)((hmacLength>>13)&0xFF);
	input[index++] = (uint8_t)((hmacLength>>5)&0xFF);
	input[index++] = (uint8_t)((hmacLength<<3)&0xFF);

	/* call the hmac function */
	hmacFunction(key, keyLength, input, inputLength, hmacLength, output);

	free(input);

	return 0;
}

/* Base32 function. Code from rfc section 5.1.6 */
void bzrtp_base32(uint32_t sas, char *output, int outputSize) {
	int i, n, shift;

	for (i=0,shift=27; i!=(outputSize-1); ++i,shift-=5) {
		n = (sas>>shift) & 31;
		output[i] = "ybndrfg8ejkmcpqxot1uwisza345h769"[n];
	}

	output[outputSize-1] = '\0';
}

/* Base256 function. Code from rfc section 5.1.6 */
void bzrtp_base256(uint32_t sas, char *output, int outputSize) {

	// generate indexes and copy the appropriate words
	int evenIndex = (sas >> 24) & 0xFF;
	if (outputSize == 32) {
		int oddIndex =  (sas >> 16) & 0xFF;
		snprintf(output, outputSize, "%s:%s", pgpWordsEven[evenIndex], pgpWordsOdd[oddIndex]);
	} else {
		snprintf(output, outputSize, "%s", pgpWordsEven[evenIndex]);
	}
}

void bzrtp_generate_incorrect_sas(uint32_t sas, char **incorrectSas, uint8_t sasAlgo) {
	int nbUsedBits;
	if(sasAlgo == ZRTP_SAS_B32) {
		nbUsedBits = 10;
	} else {
		nbUsedBits = 8;
	}
	uint32_t sasFirstPart = (sas >> (32 - nbUsedBits)) & 0x3ff;
	uint32_t sasSecondPart = (sas >> (32 - (nbUsedBits*2))) & 0x3ff;
	uint32_t randIntTab[3];
	for (int i = 0; i < 3; i++) {
		uint32_t randInt;
		bool_t sameAsOtherIncorrectSas;
		do {
			sameAsOtherIncorrectSas = FALSE;
			randInt = bctbx_random();
			for (int j = 0; j < i; j++) {
				if(((randIntTab[j]>>(32-nbUsedBits))&0x3ff) == ((randInt>>(32-nbUsedBits))&0x3ff)){
					sameAsOtherIncorrectSas = TRUE;
				}
			}
		} while ((((randInt>>(32-nbUsedBits))&0x3ff) == sasFirstPart) || (((randInt>>(32-nbUsedBits))&0x3ff) == sasSecondPart) || sameAsOtherIncorrectSas);
		randIntTab[i] = randInt;
		if (sasAlgo == ZRTP_SAS_B32) {
			incorrectSas[i] = (char *)bctbx_malloc((size_t)3*sizeof(char));
			bzrtp_base32(randInt, (char*)incorrectSas[i], 3);
		} else {
			incorrectSas[i] = (char *)bctbx_malloc((size_t)16*sizeof(char));
			bzrtp_base256(randInt, (char*)incorrectSas[i], 16);
		}
	}
}

uint32_t CRC32LookupTable[256] = {
	0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4,
	0xc79a971f, 0x35f1141c, 0x26a1e7e8, 0xd4ca64eb,
	0x8ad958cf, 0x78b2dbcc, 0x6be22838, 0x9989ab3b,
	0x4d43cfd0, 0xbf284cd3, 0xac78bf27, 0x5e133c24,
	0x105ec76f, 0xe235446c, 0xf165b798, 0x030e349b,
	0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384,
	0x9a879fa0, 0x68ec1ca3, 0x7bbcef57, 0x89d76c54,
	0x5d1d08bf, 0xaf768bbc, 0xbc267848, 0x4e4dfb4b,
	0x20bd8ede, 0xd2d60ddd, 0xc186fe29, 0x33ed7d2a,
	0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35,
	0xaa64d611, 0x580f5512, 0x4b5fa6e6, 0xb93425e5,
	0x6dfe410e, 0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa,
	0x30e349b1, 0xc288cab2, 0xd1d83946, 0x23b3ba45,
	0xf779deae, 0x05125dad, 0x1642ae59, 0xe4292d5a,
	0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a,
	0x7da08661, 0x8fcb0562, 0x9c9bf696, 0x6ef07595,
	0x417b1dbc, 0xb3109ebf, 0xa0406d4b, 0x522bee48,
	0x86e18aa3, 0x748a09a0, 0x67dafa54, 0x95b17957,
	0xcba24573, 0x39c9c670, 0x2a993584, 0xd8f2b687,
	0x0c38d26c, 0xfe53516f, 0xed03a29b, 0x1f682198,
	0x5125dad3, 0xa34e59d0, 0xb01eaa24, 0x42752927,
	0x96bf4dcc, 0x64d4cecf, 0x77843d3b, 0x85efbe38,
	0xdbfc821c, 0x2997011f, 0x3ac7f2eb, 0xc8ac71e8,
	0x1c661503, 0xee0d9600, 0xfd5d65f4, 0x0f36e6f7,
	0x61c69362, 0x93ad1061, 0x80fde395, 0x72966096,
	0xa65c047d, 0x5437877e, 0x4767748a, 0xb50cf789,
	0xeb1fcbad, 0x197448ae, 0x0a24bb5a, 0xf84f3859,
	0x2c855cb2, 0xdeeedfb1, 0xcdbe2c45, 0x3fd5af46,
	0x7198540d, 0x83f3d70e, 0x90a324fa, 0x62c8a7f9,
	0xb602c312, 0x44694011, 0x5739b3e5, 0xa55230e6,
	0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36,
	0x3cdb9bdd, 0xceb018de, 0xdde0eb2a, 0x2f8b6829,
	0x82f63b78, 0x709db87b, 0x63cd4b8f, 0x91a6c88c,
	0x456cac67, 0xb7072f64, 0xa457dc90, 0x563c5f93,
	0x082f63b7, 0xfa44e0b4, 0xe9141340, 0x1b7f9043,
	0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c,
	0x92a8fc17, 0x60c37f14, 0x73938ce0, 0x81f80fe3,
	0x55326b08, 0xa759e80b, 0xb4091bff, 0x466298fc,
	0x1871a4d8, 0xea1a27db, 0xf94ad42f, 0x0b21572c,
	0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033,
	0xa24bb5a6, 0x502036a5, 0x4370c551, 0xb11b4652,
	0x65d122b9, 0x97baa1ba, 0x84ea524e, 0x7681d14d,
	0x2892ed69, 0xdaf96e6a, 0xc9a99d9e, 0x3bc21e9d,
	0xef087a76, 0x1d63f975, 0x0e330a81, 0xfc588982,
	0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d,
	0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622,
	0x38cc2a06, 0xcaa7a905, 0xd9f75af1, 0x2b9cd9f2,
	0xff56bd19, 0x0d3d3e1a, 0x1e6dcdee, 0xec064eed,
	0xc38d26c4, 0x31e6a5c7, 0x22b65633, 0xd0ddd530,
	0x0417b1db, 0xf67c32d8, 0xe52cc12c, 0x1747422f,
	0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff,
	0x8ecee914, 0x7ca56a17, 0x6ff599e3, 0x9d9e1ae0,
	0xd3d3e1ab, 0x21b862a8, 0x32e8915c, 0xc083125f,
	0x144976b4, 0xe622f5b7, 0xf5720643, 0x07198540,
	0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90,
	0x9e902e7b, 0x6cfbad78, 0x7fab5e8c, 0x8dc0dd8f,
	0xe330a81a, 0x115b2b19, 0x020bd8ed, 0xf0605bee,
	0x24aa3f05, 0xd6c1bc06, 0xc5914ff2, 0x37faccf1,
	0x69e9f0d5, 0x9b8273d6, 0x88d28022, 0x7ab90321,
	0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e,
	0xf36e6f75, 0x0105ec76, 0x12551f82, 0xe03e9c81,
	0x34f4f86a, 0xc69f7b69, 0xd5cf889d, 0x27a40b9e,
	0x79b737ba, 0x8bdcb4b9, 0x988c474d, 0x6ae7c44e,
	0xbe2da0a5, 0x4c4623a6, 0x5f16d052, 0xad7d5351
};

/* CRC32 Polynomial 0x1EDC6F41 in reverse bit order so
 * used the reversed one 0x82F63B78 to compute the table */
uint32_t bzrtp_CRC32(uint8_t *input, uint16_t length) {

	int i;

	/* code used to generate the lookup table but it's faster to store it */
	/*
	int j;
	uint32_t CRC32LookupTable[256];
	for (i = 0; i <= 0xFF; i++) {
		uint32_t crcT = i;
		for (j = 0; j < 8; j++) {
			crcT = (crcT >> 1) ^ ((crcT & 1) * 0x82F63B78);
		}
		CRC32LookupTable[i] = crcT;
	}

	for (i=0; i<256; i+=4) {
			printf("0x%08lx, 0x%08lx, 0x%08lx, 0x%08lx,\n", (long unsigned int)CRC32LookupTable[i], (long unsigned int)CRC32LookupTable[i+1], (long unsigned int)CRC32LookupTable[i+2], (long unsigned int)CRC32LookupTable[i+3]);
	}*/

	uint32_t crc = 0xFFFFFFFF;
	for (i=0; i<length; i++) {
		crc = (crc >> 8) ^ CRC32LookupTable[(crc & 0xFF) ^ input[i]];
	}

	crc =~crc;

	/* swap the byte order */
	return ((crc&0xFF000000)>>24)|((crc&0x00FF0000)>>8)|((crc&0x0000FF00)<<8)|((crc&0x000000FF)<<24);
}

bool_t bzrtp_isPostQuantum(uint8_t keyAgreementAlgo) {
	switch (keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_KYB1:
	case ZRTP_KEYAGREEMENT_KYB2:
	case ZRTP_KEYAGREEMENT_KYB3:
	case ZRTP_KEYAGREEMENT_HQC1:
	case ZRTP_KEYAGREEMENT_HQC2:
	case ZRTP_KEYAGREEMENT_HQC3:
	case ZRTP_KEYAGREEMENT_K255_KYB512:
	case ZRTP_KEYAGREEMENT_K255_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
	case ZRTP_KEYAGREEMENT_K448_HQC256:
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
		return TRUE;
	default:
		return false;
	}
}

/*
 * @brief select a key agreement algorithm from the one available in context and the one provided by
 * peer in Hello Message as described in rfc section 4.1.2
 * - other algorithm are selected according to availability and selected key agreement as described in
 *   rfc section 5.1.5
 * The other algorithm choice will finally be set by the endpoint acting as initiator in the commit packet
 *
 * @param[in,out]	zrtpContext			The context contains the list of available algo and is set with the selected ones and associated functions
 * @param[in]		peerHelloMessage	The peer hello message containing his set of available algos
 *
 * return			0 on succes, error code otherwise
 *
 */
int bzrtp_cryptoAlgoAgreement(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext, bzrtpHelloMessage_t *peerHelloMessage) {
	uint8_t selfCommonKeyAgreementType[7];
	uint8_t peerCommonKeyAgreementType[7];
	uint8_t commonKeyAgreementTypeNumber = 0;
	uint8_t commonCipherType[7];
	uint8_t commonCipherTypeNumber;
	uint8_t commonHashType[7];
	uint8_t commonHashTypeNumber;
	uint8_t commonAuthTagType[7];
	uint8_t commonAuthTagTypeNumber;
	uint8_t commonSasType[7];
	uint8_t commonSasTypeNumber;

	/* check context and Message */
	if (zrtpContext == NULL) {
		return ZRTP_CRYPTOAGREEMENT_INVALIDCONTEXT;
	}

	if (zrtpContext->kc == 0) {
		return ZRTP_CRYPTOAGREEMENT_INVALIDCONTEXT;
	}

	if (peerHelloMessage == NULL) {
		return ZRTP_CRYPTOAGREEMENT_INVALIDMESSAGE;
	}

	if (peerHelloMessage->kc == 0) {
		return ZRTP_CRYPTOAGREEMENT_INVALIDMESSAGE;
	}

	/* now check what is in common in self and peer order */
	/* self ordering: get the common list in the self order of preference */
	commonKeyAgreementTypeNumber = bzrtp_selectCommonAlgo(zrtpContext->supportedKeyAgreement, zrtpContext->kc, peerHelloMessage->supportedKeyAgreement, peerHelloMessage->kc,  selfCommonKeyAgreementType);

	/* check we have something in common */
	if (commonKeyAgreementTypeNumber == 0) { /* this shall never bee true as all ZRTP endpoint MUST support at least DH3k */
		return ZRTP_CRYPTOAGREEMENT_NOCOMMONALGOFOUND;
	}

	/* peer ordering: get the common list in the peer order of preference */
	bzrtp_selectCommonAlgo(peerHelloMessage->supportedKeyAgreement, peerHelloMessage->kc, zrtpContext->supportedKeyAgreement, zrtpContext->kc, peerCommonKeyAgreementType);

	/* if the first choices are the same for both, select it */
	if (selfCommonKeyAgreementType[0] == peerCommonKeyAgreementType[0]) {
		zrtpChannelContext->keyAgreementAlgo = selfCommonKeyAgreementType[0];
	} else { /* select the fastest of the two algoritm. Order is "DH2k", "E255", "EC25", "E448", "DH3k", "EC38", "EC52" is defined by value order from bzrtp.h */
		if (peerCommonKeyAgreementType[0]<selfCommonKeyAgreementType[0]) { /* mapping to uint8_t is defined to have them in the fast to slow order */
			zrtpChannelContext->keyAgreementAlgo = peerCommonKeyAgreementType[0];
		} else {
			zrtpChannelContext->keyAgreementAlgo = selfCommonKeyAgreementType[0];
		}
	}

	/* now we shall select others algos among the availables and set them into the context, these choices may be
	 * bypassed if we assume the receptor role as the initiator's commit will have the final word on the algo
	 * selection */

	/*** Cipher block algorithm ***/
	/* get the self cipher types availables */
	commonCipherTypeNumber = bzrtp_selectCommonAlgo(zrtpContext->supportedCipher, zrtpContext->cc, peerHelloMessage->supportedCipher, peerHelloMessage->cc,  commonCipherType);

	if (commonCipherTypeNumber == 0) {/* This shall never happend but... */
		return ZRTP_CRYPTOAGREEMENT_INVALIDCIPHER;
	}
	/* rfc section 5.1.5 specifies that if EC38 is choosen we SHOULD use AES256 or AES192 */
	/* if a post quantum algorithm is choosen we SHOULD use AES256 */
	if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_EC38 || bzrtp_isPostQuantum(zrtpChannelContext->keyAgreementAlgo)
			|| zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X448 || zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_K448) {
		int i=0;

		zrtpChannelContext->cipherAlgo = ZRTP_UNSET_ALGO;
		/* is AES3 available */
		while (i<commonCipherTypeNumber && zrtpChannelContext->cipherAlgo == ZRTP_UNSET_ALGO) {
			if (commonCipherType[i] == ZRTP_CIPHER_AES3) {
				zrtpChannelContext->cipherAlgo = ZRTP_CIPHER_AES3;
			}
			i++;
		}
		/* is AES2 available */
		i=0;
		while (i<commonCipherTypeNumber && zrtpChannelContext->cipherAlgo == ZRTP_UNSET_ALGO) {
			if (commonCipherType[i] == ZRTP_CIPHER_AES2) {
				zrtpChannelContext->cipherAlgo = ZRTP_CIPHER_AES2;
			}
			i++;
		}
		/* is AES1 available */
		if (zrtpChannelContext->cipherAlgo == ZRTP_UNSET_ALGO) {
			zrtpChannelContext->cipherAlgo = ZRTP_CIPHER_AES1;

		}
	} else { /* no restrictions, pick the first one */
		zrtpChannelContext->cipherAlgo = commonCipherType[0];
	}

	/*** Hash algorithm ***/
	/* get the self hash types availables */
	commonHashTypeNumber = bzrtp_selectCommonAlgo(zrtpContext->supportedHash, zrtpContext->hc, peerHelloMessage->supportedHash, peerHelloMessage->hc, commonHashType);
	if (commonHashTypeNumber == 0) {/* This shall never happend but... */
		return ZRTP_CRYPTOAGREEMENT_INVALIDHASH;
	}

	/* the rfc section 5.1.5 specifies that if EC38 is choosen we MUST use SHA384 */
	/* hier we implement as : if EC38 is choosen we SHOULD use SHA512 (or SHA384) */
	/* if a post quantum algorithm or X448 is choosen we SHOULD use SHA512 (or SHA384) */
	if (zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_EC38 || bzrtp_isPostQuantum(zrtpChannelContext->keyAgreementAlgo)
			|| zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_X448 || zrtpChannelContext->keyAgreementAlgo == ZRTP_KEYAGREEMENT_K448) {
		int i=0;

		zrtpChannelContext->hashAlgo = ZRTP_UNSET_ALGO;
		/* is S512 available */
		while (i<commonHashTypeNumber && zrtpChannelContext->hashAlgo == ZRTP_UNSET_ALGO) {
			if (commonHashType[i] == ZRTP_HASH_S512) {
				zrtpChannelContext->hashAlgo = ZRTP_HASH_S512;
			}
			i++;
		}
		/* is S384 available */
		i=0;
		while (i<commonHashTypeNumber && zrtpChannelContext->hashAlgo == ZRTP_UNSET_ALGO) {
			if (commonHashType[i] == ZRTP_HASH_S384) {
				zrtpChannelContext->hashAlgo = ZRTP_HASH_S384;
			}
			i++;
		}
		/* is S256 available */
		if (zrtpChannelContext->hashAlgo == ZRTP_UNSET_ALGO) {
			zrtpChannelContext->hashAlgo = ZRTP_HASH_S256;
		}
	} else { /* no restrictions, pick the first one */
		zrtpChannelContext->hashAlgo = commonHashType[0];
	}

	/*** Authentication Tag algorithm ***/
	/* get the self authentication tag types availables */
	commonAuthTagTypeNumber = bzrtp_selectCommonAlgo(zrtpContext->supportedAuthTag, zrtpContext->ac, peerHelloMessage->supportedAuthTag, peerHelloMessage->ac, commonAuthTagType);
	if (commonAuthTagTypeNumber == 0) {/* This shall never happend but... */
		return ZRTP_CRYPTOAGREEMENT_INVALIDAUTHTAG;
	}
	zrtpChannelContext->authTagAlgo = commonAuthTagType[0];

	/*** Sas algorithm ***/
	/* get the self Sas rendering types availables */
	commonSasTypeNumber = bzrtp_selectCommonAlgo(zrtpContext->supportedSas, zrtpContext->sc, peerHelloMessage->supportedSas, peerHelloMessage->sc, commonSasType);
	if (commonSasTypeNumber == 0) {/* This shall never happend but... */
		return ZRTP_CRYPTOAGREEMENT_INVALIDSAS;
	}
	zrtpChannelContext->sasAlgo = commonSasType[0];

	/* update the function pointers */
	return bzrtp_updateCryptoFunctionPointers(zrtpChannelContext);
}

/*
 * @brief Update context crypto function pointer according to related values of choosen algorithms fields (hashAlgo, cipherAlgo, etc..)
 * The associated length are updated too
 *
 * @param[in,out]	context		The bzrtp context to be updated
 *
 * @return			0 on succes
 */
int bzrtp_updateCryptoFunctionPointers(bzrtpChannelContext_t *zrtpChannelContext) {
	if (zrtpChannelContext==NULL) {
		return ZRTP_CRYPTOAGREEMENT_INVALIDCONTEXT;
	}

	/* Hash algo */
	switch (zrtpChannelContext->hashAlgo) {
	case ZRTP_HASH_S256 :
		zrtpChannelContext->hashFunction = bctbx_sha256;
		zrtpChannelContext->hmacFunction = bctbx_hmacSha256;
		zrtpChannelContext->hashLength = 32;
		break;
	case ZRTP_HASH_S384 :
		zrtpChannelContext->hashFunction = bctbx_sha384;
		zrtpChannelContext->hmacFunction = bctbx_hmacSha384;
		zrtpChannelContext->hashLength = 48;
		break;
	case ZRTP_HASH_S512 :
		zrtpChannelContext->hashFunction = bctbx_sha512;
		zrtpChannelContext->hmacFunction = bctbx_hmacSha512;
		zrtpChannelContext->hashLength = 64;
		break;
	case ZRTP_UNSET_ALGO :
		zrtpChannelContext->hashFunction = NULL;
		zrtpChannelContext->hmacFunction = NULL;
		zrtpChannelContext->hashLength = 0;
		break;
	default:
		return ZRTP_CRYPTOAGREEMENT_INVALIDHASH;
		break;
	}

	/* CipherBlock algo */
	switch (zrtpChannelContext->cipherAlgo) {
	case ZRTP_CIPHER_AES1 :
		zrtpChannelContext->cipherEncryptionFunction = bctbx_aes128CfbEncrypt;
		zrtpChannelContext->cipherDecryptionFunction = bctbx_aes128CfbDecrypt;
		zrtpChannelContext->cipherKeyLength = 16;
		break;
	case ZRTP_CIPHER_AES3 :
		zrtpChannelContext->cipherEncryptionFunction = bctbx_aes256CfbEncrypt;
		zrtpChannelContext->cipherDecryptionFunction = bctbx_aes256CfbDecrypt;
		zrtpChannelContext->cipherKeyLength = 32;
		break;
	case ZRTP_UNSET_ALGO :
		zrtpChannelContext->cipherEncryptionFunction = NULL;
		zrtpChannelContext->cipherDecryptionFunction = NULL;
		zrtpChannelContext->cipherKeyLength = 0;
		break;
	default:
		return ZRTP_CRYPTOAGREEMENT_INVALIDCIPHER;
		break;
	}

	/* Key agreement algo : is directly managed in the parse and state machine, nothing to do, just check it is valid */
	switch (zrtpChannelContext->keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_DH2k:
	case ZRTP_KEYAGREEMENT_DH3k:
	case ZRTP_KEYAGREEMENT_X255:
	case ZRTP_KEYAGREEMENT_X448:
	case ZRTP_KEYAGREEMENT_K255:
	case ZRTP_KEYAGREEMENT_K448:
	case ZRTP_KEYAGREEMENT_KYB1:
	case ZRTP_KEYAGREEMENT_KYB2:
	case ZRTP_KEYAGREEMENT_KYB3:
	case ZRTP_KEYAGREEMENT_HQC1:
	case ZRTP_KEYAGREEMENT_HQC2:
	case ZRTP_KEYAGREEMENT_HQC3:
	case ZRTP_KEYAGREEMENT_K255_KYB512:
	case ZRTP_KEYAGREEMENT_K255_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
	case ZRTP_KEYAGREEMENT_K448_HQC256:
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
	case ZRTP_KEYAGREEMENT_Mult:
	case ZRTP_KEYAGREEMENT_Prsh:
	case ZRTP_UNSET_ALGO :
		break;
	default:
		return ZRTP_CRYPTOAGREEMENT_INVALIDCIPHER;
		break;
	}

	/* SAS rendering algo */
	switch(zrtpChannelContext->sasAlgo) {
	case ZRTP_SAS_B32:
		zrtpChannelContext->sasFunction = bzrtp_base32;
		// extend 4 byte b32 length to include null terminator
		zrtpChannelContext->sasLength = 5;
		break;
	case ZRTP_SAS_B256:
		zrtpChannelContext->sasFunction = bzrtp_base256;
		zrtpChannelContext->sasLength = 32;
		break;
	case ZRTP_UNSET_ALGO :
		zrtpChannelContext->sasFunction = NULL;
		zrtpChannelContext->sasLength = 0;
		break;
	default:
		return ZRTP_CRYPTOAGREEMENT_INVALIDSAS;
		break;
	}
	return 0;
}

#define BITS_PRO_INT 8*sizeof(int)
#define BITMASK_256_SIZE 256/BITS_PRO_INT
#define BITMASK_256_SET_ZERO(bitmask) memset(bitmask, 0, sizeof(int)*BITMASK_256_SIZE)
#define BITMASK_256_SET(bitmask, value) bitmask[value/BITS_PRO_INT] |= 1 << (value % BITS_PRO_INT)
#define BITMASK_256_UNSET(bitmask, value) bitmask[value/BITS_PRO_INT] &= ~(1 << (value % BITS_PRO_INT))
#define BITMASK_256_CHECK(bitmask, value) (bitmask[value/BITS_PRO_INT] & 1 << (value % BITS_PRO_INT))

/**
 * @brief Select common algorithm from the given array where algo are represented by their 4 chars string defined in rfc section 5.1.2 to 5.1.6
 * Master array is the one given the preference order
 * All algo are designed by their uint8_t mapped values
 *
 * @param[in]	masterArray	 		The ordered available algo, result will follow this ordering
 * @param[in]	masterArrayLength	Number of valids element in the master array
 * @param[in]	slaveArray	 		The available algo, order is not taken in account
 * @param[in]	slaveArrayLength	Number of valids element in the slave array
 * @param[out]	commonArray	 		Common algorithms found, max size 7
 *
 * @return		the number of common algorithms found
 */
uint8_t bzrtp_selectCommonAlgo(uint8_t masterArray[7], uint8_t masterArrayLength, uint8_t *slaveArray, uint8_t slaveArrayLength, uint8_t commonArray[7]) {
	int i;
	uint8_t commonLength = 0;
	int algosBitmap[BITMASK_256_SIZE];

	BITMASK_256_SET_ZERO(algosBitmap);
	for (i=0; i<slaveArrayLength; i++) {
		BITMASK_256_SET(algosBitmap, slaveArray[i]);
	}

	for (i=0; i<masterArrayLength; i++) {
		if (BITMASK_256_CHECK(algosBitmap, masterArray[i])) {
			BITMASK_256_UNSET(algosBitmap, masterArray[i]);
			commonArray[commonLength] = masterArray[i];
			commonLength++;

			if (commonLength == 7) {
				return commonLength;
			}
		}
	}

	return commonLength;
}

/**
 * @brief add mandatory crypto functions if they are not already included
 * - Hash function
 * - Cipher Block
 * - Auth Tag
 * - Key agreement
 * - SAS
 *
 * @param[in]		algoType		mapped to defines, must be in [ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE, ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE]
 * @param[in,out]	algoTypes		mapped to uint8_t value of the 4 char strings giving the algo types as string according to rfc section 5.1.2 to 5.1.6
 * @param[in,out]	algoTypesCount	number of algo types
 */
void bzrtp_addMandatoryCryptoTypesIfNeeded(uint8_t algoType, uint8_t algoTypes[7], uint8_t *algoTypesCount)
{
	int i, j;
	int algosBitmask[BITMASK_256_SIZE];
	int missingBitmask[BITMASK_256_SIZE];
	uint8_t mandatoryTypes[7];
	const uint8_t mandatoryTypesCount = bzrtpUtils_getMandatoryCryptoTypes(algoType, mandatoryTypes);
	uint8_t missingTypesCount = mandatoryTypesCount;

	BITMASK_256_SET_ZERO(missingBitmask);
	BITMASK_256_SET_ZERO(algosBitmask);

	for(i=0; i<mandatoryTypesCount; i++) {
		BITMASK_256_SET(missingBitmask, mandatoryTypes[i]);
	}

	for (i=0,j=0; j<7 && (i<*algoTypesCount); i++) {
		/*
		 * If we only have space left for missing crypto algos, only add them.
		 * Make sure we do not add elements twice.
		 */
		if ((j + missingTypesCount < 7 || BITMASK_256_CHECK(missingBitmask, algoTypes[i])) && !BITMASK_256_CHECK(algosBitmask, algoTypes[i])) {
			BITMASK_256_SET(algosBitmask, algoTypes[i]);
			algoTypes[j++] = algoTypes[i];

			if (BITMASK_256_CHECK(missingBitmask, algoTypes[i])) {
				BITMASK_256_UNSET(missingBitmask, algoTypes[i]);
				missingTypesCount--;
			}
		}
	}

	/* add missing crypto types */
	for (i=0; i<7 && missingTypesCount>0 && i<mandatoryTypesCount; i++) {
		if (BITMASK_256_CHECK(missingBitmask, mandatoryTypes[i])) {
			algoTypes[j++] = mandatoryTypes[i];
			missingTypesCount--;
		}
	}
	*algoTypesCount = j;
}

/*
 * @brief Map the string description of algo type to an int defined in cryptoWrapper.h
 *
 * @param[in] algoType		A 4 chars string containing the algo type as listed in rfc sections 5.1.2 to 5.1.6
 * @param[in] algoFamily	The integer mapped algo family (ZRTP_HASH_TYPE, ZRTP_CIPHERBLOCK_TYPE, ZRTP_AUTHTAG_TYPE,
 * 							ZRTP_KEYAGREEMENT_TYPE or ZRTP_SAS_TYPE)
 *
 * @return 		The int value mapped to the algo type, ZRTP_UNSET_ALGO on error
 */
uint8_t bzrtp_cryptoAlgoTypeStringToInt(uint8_t algoType[4], uint8_t algoFamily) {
	switch (algoFamily) {
	case ZRTP_HASH_TYPE:
	{
		if (memcmp(algoType, "S256", 4) == 0) {
			return ZRTP_HASH_S256;
		} else if (memcmp(algoType, "S384", 4) == 0) {
			return ZRTP_HASH_S384;
		} else if (memcmp(algoType, "S512", 4) == 0) {
			return ZRTP_HASH_S512;
		} else if (memcmp(algoType, "N256", 4) == 0) {
			return ZRTP_HASH_N256;
		} else if (memcmp(algoType, "N384", 4) == 0) {
			return ZRTP_HASH_N384;
		}
		return ZRTP_UNSET_ALGO;
	}
		break;

	case ZRTP_CIPHERBLOCK_TYPE:
	{
		if (memcmp(algoType, "AES1", 4) == 0) {
			return ZRTP_CIPHER_AES1;
		} else if (memcmp(algoType, "AES2", 4) == 0) {
			return ZRTP_CIPHER_AES2;
		} else if (memcmp(algoType, "AES3", 4) == 0) {
			return ZRTP_CIPHER_AES3;
		} else if (memcmp(algoType, "2FS1", 4) == 0) {
			return ZRTP_CIPHER_2FS1;
		} else if (memcmp(algoType, "2FS2", 4) == 0) {
			return ZRTP_CIPHER_2FS2;
		} else if (memcmp(algoType, "2FS3", 4) == 0) {
			return ZRTP_CIPHER_2FS3;
		}
		return ZRTP_UNSET_ALGO;
	}
		break;

	case ZRTP_AUTHTAG_TYPE:
	{
		if (memcmp(algoType, "HS32", 4) == 0) {
			return ZRTP_AUTHTAG_HS32;
		} else if (memcmp(algoType, "HS80", 4) == 0) {
			return ZRTP_AUTHTAG_HS80;
		} else if (memcmp(algoType, "SK32", 4) == 0) {
			return ZRTP_AUTHTAG_SK32;
		} else if (memcmp(algoType, "SK64", 4) == 0) {
			return ZRTP_AUTHTAG_SK64;
		} else if (memcmp(algoType, "GCM ", 4) == 0) {
			return ZRTP_AUTHTAG_GCM;
		}
		return ZRTP_UNSET_ALGO;
	}
		break;

	case ZRTP_KEYAGREEMENT_TYPE:
	{
		if (memcmp(algoType, "DH3k", 4) == 0) {
			return ZRTP_KEYAGREEMENT_DH3k;
		} else if (memcmp(algoType, "DH2k", 4) == 0) {
			return ZRTP_KEYAGREEMENT_DH2k;
		} else if (memcmp(algoType, "X255", 4) == 0) {
			return ZRTP_KEYAGREEMENT_X255;
		} else if (memcmp(algoType, "K255", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K255;
		} else if (memcmp(algoType, "EC25", 4) == 0) {
			return ZRTP_KEYAGREEMENT_EC25;
		} else if (memcmp(algoType, "X448", 4) == 0) {
			return ZRTP_KEYAGREEMENT_X448;
		} else if (memcmp(algoType, "K448", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K448;
		} else if (memcmp(algoType, "EC38", 4) == 0) {
			return ZRTP_KEYAGREEMENT_EC38;
		} else if (memcmp(algoType, "EC52", 4) == 0) {
			return ZRTP_KEYAGREEMENT_EC52;
		} else if (memcmp(algoType, "KYB1", 4) == 0) {
			return ZRTP_KEYAGREEMENT_KYB1;
		} else if (memcmp(algoType, "KYB2", 4) == 0) {
			return ZRTP_KEYAGREEMENT_KYB2;
		} else if (memcmp(algoType, "KYB3", 4) == 0) {
			return ZRTP_KEYAGREEMENT_KYB3;
		// HQC implementation used to be the round 3 submission (2020/10/11)
		// -> it advertised itself as HQB<X> (and XKQ<x> for the X/Kyber/HQC hybrids
		// Current HQC implementation is the one from round 4 submission (2024/02/23), incompatible with the previous one
		// -> it advertises itself as HQD<x> and XKR<x> for the X/Kyber/HQC hybrids)
		} else if (memcmp(algoType, "HQD1", 4) == 0) {
			return ZRTP_KEYAGREEMENT_HQC1;
		} else if (memcmp(algoType, "HQD2", 4) == 0) {
			return ZRTP_KEYAGREEMENT_HQC2;
		} else if (memcmp(algoType, "HQD3", 4) == 0) {
			return ZRTP_KEYAGREEMENT_HQC3;
		} else if (memcmp(algoType, "X1K1", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K255_KYB512;
		} else if (memcmp(algoType, "X1H1", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K255_HQC128;
		} else if (memcmp(algoType, "X3K3", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K448_KYB1024;
		} else if (memcmp(algoType, "X3H3", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K448_HQC256;
		} else if (memcmp(algoType, "XKR1", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K255_KYB512_HQC128;
		} else if (memcmp(algoType, "XKR3", 4) == 0) {
			return ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256;
		} else if (memcmp(algoType, "Prsh", 4) == 0) {
			return ZRTP_KEYAGREEMENT_Prsh;
		} else if (memcmp(algoType, "Mult", 4) == 0) {
			return ZRTP_KEYAGREEMENT_Mult;
		}
		return ZRTP_UNSET_ALGO;
	}
		break;

	case ZRTP_SAS_TYPE :
	{
		if (memcmp(algoType, "B32 ", 4) == 0) {
			return ZRTP_SAS_B32;
		} else if (memcmp(algoType, "B256", 4) == 0) {
			return ZRTP_SAS_B256;
		}
		return ZRTP_UNSET_ALGO;
	}
		break;
	default:
		return ZRTP_UNSET_ALGO;
	}
}

/*
 * @brief Unmap the string description of algo type to an int defined in cryptoWrapper.h
 *
 * @param[in] algoTypeInt	The integer algo type defined in crypoWrapper.h
 * @param[in] algoFamily	The string code for the algorithm as defined in rfc 5.1.2 to 5.1.6
 */
void bzrtp_cryptoAlgoTypeIntToString(uint8_t algoTypeInt, uint8_t algoTypeString[4]) {
	switch (algoTypeInt) {
	case ZRTP_HASH_S256:
		memcpy(algoTypeString, "S256", 4);
		break;
	case ZRTP_HASH_S384:
		memcpy(algoTypeString, "S384", 4);
		break;
	case ZRTP_HASH_S512:
		memcpy(algoTypeString, "S512", 4);
		break;
	case ZRTP_HASH_N256:
		memcpy(algoTypeString, "N256", 4);
		break;
	case ZRTP_HASH_N384:
		memcpy(algoTypeString, "N384", 4);
		break;
	case ZRTP_CIPHER_AES1:
		memcpy(algoTypeString, "AES1", 4);
		break;
	case ZRTP_CIPHER_AES2:
		memcpy(algoTypeString, "AES2", 4);
		break;
	case ZRTP_CIPHER_AES3:
		memcpy(algoTypeString, "AES3", 4);
		break;
	case ZRTP_CIPHER_2FS1:
		memcpy(algoTypeString, "2FS1", 4);
		break;
	case ZRTP_CIPHER_2FS2:
		memcpy(algoTypeString, "2FS2", 4);
		break;
	case ZRTP_CIPHER_2FS3:
		memcpy(algoTypeString, "2FS3", 4);
		break;
	case ZRTP_AUTHTAG_HS32:
		memcpy(algoTypeString, "HS32", 4);
		break;
	case ZRTP_AUTHTAG_HS80:
		memcpy(algoTypeString, "HS80", 4);
		break;
	case ZRTP_AUTHTAG_SK32:
		memcpy(algoTypeString, "SK32", 4);
		break;
	case ZRTP_AUTHTAG_SK64:
		memcpy(algoTypeString, "SK64", 4);
		break;
	case ZRTP_AUTHTAG_GCM:
		memcpy(algoTypeString, "GCM ", 4);
		break;
	case ZRTP_KEYAGREEMENT_DH2k:
		memcpy(algoTypeString, "DH2k", 4);
		break;
	case ZRTP_KEYAGREEMENT_X255:
		memcpy(algoTypeString, "X255", 4);
		break;
	case ZRTP_KEYAGREEMENT_K255:
		memcpy(algoTypeString, "K255", 4);
		break;
	case ZRTP_KEYAGREEMENT_EC25:
		memcpy(algoTypeString, "EC25", 4);
		break;
	case ZRTP_KEYAGREEMENT_X448:
		memcpy(algoTypeString, "X448", 4);
		break;
	case ZRTP_KEYAGREEMENT_K448:
		memcpy(algoTypeString, "K448", 4);
		break;
	case ZRTP_KEYAGREEMENT_DH3k:
		memcpy(algoTypeString, "DH3k", 4);
		break;
	case ZRTP_KEYAGREEMENT_EC38:
		memcpy(algoTypeString, "EC38", 4);
		break;
	case ZRTP_KEYAGREEMENT_EC52:
		memcpy(algoTypeString, "EC52", 4);
		break;
	case ZRTP_KEYAGREEMENT_KYB1:
		memcpy(algoTypeString, "KYB1", 4);
		break;
	case ZRTP_KEYAGREEMENT_KYB2:
		memcpy(algoTypeString, "KYB2", 4);
		break;
	case ZRTP_KEYAGREEMENT_KYB3:
		memcpy(algoTypeString, "KYB3", 4);
		break;
	// HQC implementation used to be the round 3 submission (2020/10/11)
	// -> it advertised itself as HQB<X> (and XKQ<x> for the X/Kyber/HQC hybrids
	// Current HQC implementation is the one from round 4 submission (2024/02/23), incompatible with the previous one
	// -> it advertises itself as HQD<x> and XKR<x> for the X/Kyber/HQC hybrids)
	case ZRTP_KEYAGREEMENT_HQC1:
		memcpy(algoTypeString, "HQD1", 4);
		break;
	case ZRTP_KEYAGREEMENT_HQC2:
		memcpy(algoTypeString, "HQD2", 4);
		break;
	case ZRTP_KEYAGREEMENT_HQC3:
		memcpy(algoTypeString, "HQD3", 4);
		break;
	case ZRTP_KEYAGREEMENT_K255_KYB512:
		memcpy(algoTypeString, "X1K1", 4);
		break;
	case ZRTP_KEYAGREEMENT_K255_HQC128:
		memcpy(algoTypeString, "X1H1", 4);
		break;
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
		memcpy(algoTypeString, "X3K3", 4);
		break;
	case ZRTP_KEYAGREEMENT_K448_HQC256:
		memcpy(algoTypeString, "X3H3", 4);
		break;
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
		memcpy(algoTypeString, "XKR1", 4);
		break;
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
		memcpy(algoTypeString, "XKR3", 4);
		break;
	case ZRTP_KEYAGREEMENT_Prsh:
		memcpy(algoTypeString, "Prsh", 4);
		break;
	case ZRTP_KEYAGREEMENT_Mult:
		memcpy(algoTypeString, "Mult", 4);
		break;
	case ZRTP_SAS_B32:
		memcpy(algoTypeString, "B32 ", 4);
		break;
	case ZRTP_SAS_B256:
		memcpy(algoTypeString, "B256", 4);
		break;
	default :
		memcpy(algoTypeString, "NSET", 4);
		break;
	}
}

/*
 * @brief Destroy a key by setting it to a random number
 * Key is not freed, caller must deal with memory management
 *
 * @param[in,out]	key			The key to be destroyed
 * @param[in]		keyLength	The keyLength in bytes
 * @param[in]		rngContext	The context for RNG
 */
void bzrtp_DestroyKey(uint8_t *key, size_t keyLength, void *rngContext) {
	if (key != NULL) {
		bctbx_rng_get(static_cast<bctbx_rng_context_t *>(rngContext), key, keyLength);
	}
}

void bzrtp_destroyKeyMaterial(bzrtpContext_t *zrtpContext, bzrtpChannelContext_t *zrtpChannelContext){
	/* destroy and free the key buffers */
	bzrtp_DestroyKey(zrtpChannelContext->s0, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->KDFContext, zrtpChannelContext->KDFContextLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->mackeyi, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->mackeyr, zrtpChannelContext->hashLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->zrtpkeyi, zrtpChannelContext->cipherKeyLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->zrtpkeyr, zrtpChannelContext->cipherKeyLength, zrtpContext->RNGContext);

	free(zrtpChannelContext->s0);
	free(zrtpChannelContext->KDFContext);
	free(zrtpChannelContext->mackeyi);
	free(zrtpChannelContext->mackeyr);
	free(zrtpChannelContext->zrtpkeyi);
	free(zrtpChannelContext->zrtpkeyr);

	zrtpChannelContext->s0=NULL;
	zrtpChannelContext->KDFContext=NULL;
	zrtpChannelContext->mackeyi=NULL;
	zrtpChannelContext->mackeyr=NULL;
	zrtpChannelContext->zrtpkeyi=NULL;
	zrtpChannelContext->zrtpkeyr=NULL;

	/* destroy and free the srtp and sas struture */
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.selfSrtpKey, zrtpChannelContext->srtpSecrets.selfSrtpKeyLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.selfSrtpSalt, zrtpChannelContext->srtpSecrets.selfSrtpSaltLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.peerSrtpKey, zrtpChannelContext->srtpSecrets.peerSrtpKeyLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey(zrtpChannelContext->srtpSecrets.peerSrtpSalt, zrtpChannelContext->srtpSecrets.peerSrtpSaltLength, zrtpContext->RNGContext);
	bzrtp_DestroyKey((uint8_t *)zrtpChannelContext->srtpSecrets.sas, zrtpChannelContext->srtpSecrets.sasLength, zrtpContext->RNGContext);


	free(zrtpChannelContext->srtpSecrets.selfSrtpKey);
	free(zrtpChannelContext->srtpSecrets.selfSrtpSalt);
	free(zrtpChannelContext->srtpSecrets.peerSrtpKey);
	free(zrtpChannelContext->srtpSecrets.peerSrtpSalt);
	free(zrtpChannelContext->srtpSecrets.sas);

	for (size_t i = 0; i < 3; i++) {
		bctbx_free(zrtpChannelContext->srtpSecrets.incorrectSas[i]);
		zrtpChannelContext->srtpSecrets.incorrectSas[i] = NULL;
	}

	zrtpChannelContext->srtpSecrets.selfSrtpKey=NULL;
	zrtpChannelContext->srtpSecrets.selfSrtpSalt=NULL;
	zrtpChannelContext->srtpSecrets.peerSrtpKey=NULL;
	zrtpChannelContext->srtpSecrets.peerSrtpSalt=NULL;
	zrtpChannelContext->srtpSecrets.sas = NULL;
}

/**
 * Returns public key size in bytes for any supported KEM algo
 * @param[in] keyAgreementAlgo
 *
 * @return The public key size in bytes
 */
static uint16_t bzrt_getKEMPublicKeyLength(uint8_t keyAgreementAlgo) {
#ifdef HAVE_BCTBXPQ
	switch (keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_KYB1:
		return bctoolbox::KYBER512::pkSize;
	case ZRTP_KEYAGREEMENT_KYB2:
		return bctoolbox::KYBER768::pkSize;
	case ZRTP_KEYAGREEMENT_KYB3:
		return bctoolbox::KYBER1024::pkSize;
	case ZRTP_KEYAGREEMENT_HQC1:
		return bctoolbox::HQC128::pkSize;
	case ZRTP_KEYAGREEMENT_HQC2:
		return bctoolbox::HQC192::pkSize;
	case ZRTP_KEYAGREEMENT_HQC3:
		return bctoolbox::HQC256::pkSize;
	case ZRTP_KEYAGREEMENT_K255:
		return bctoolbox::K25519::pkSize;
	case ZRTP_KEYAGREEMENT_K448:
		return bctoolbox::K448::pkSize;
	case ZRTP_KEYAGREEMENT_K255_KYB512:
		return bctoolbox::K25519::pkSize + bctoolbox::KYBER512::pkSize;
	case ZRTP_KEYAGREEMENT_K255_HQC128:
		return bctoolbox::K25519::pkSize + bctoolbox::HQC128::pkSize;
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
		return bctoolbox::K448::pkSize + bctoolbox::KYBER1024::pkSize;
	case ZRTP_KEYAGREEMENT_K448_HQC256:
		return bctoolbox::K448::pkSize + bctoolbox::HQC256::pkSize;
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
		return bctoolbox::K25519::pkSize + bctoolbox::KYBER512::pkSize + bctoolbox::HQC128::pkSize;
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
		return bctoolbox::K448::pkSize + bctoolbox::KYBER1024::pkSize + bctoolbox::HQC256::pkSize;
	default:
		return 0;
	}
#else /* HAVE_BCTBXPQ */
	return 0;
#endif /* HAVE_BCTBXPQ */
}

/**
 * Returns cipher text size in bytes for any supported KEM algo
 * @param[in] keyAgreementAlgo
 *
 * @return The cipher text size in bytes
 */
static uint16_t bzrt_getKEMCipherTextLength(uint8_t keyAgreementAlgo) {
#ifdef HAVE_BCTBXPQ
	switch (keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_KYB1:
		return bctoolbox::KYBER512::ctSize;
	case ZRTP_KEYAGREEMENT_KYB2:
		return bctoolbox::KYBER768::ctSize;
	case ZRTP_KEYAGREEMENT_KYB3:
		return bctoolbox::KYBER1024::ctSize;
	case ZRTP_KEYAGREEMENT_HQC1:
		return bctoolbox::HQC128::ctSize;
	case ZRTP_KEYAGREEMENT_HQC2:
		return bctoolbox::HQC192::ctSize;
	case ZRTP_KEYAGREEMENT_HQC3:
		return bctoolbox::HQC256::ctSize;
	case ZRTP_KEYAGREEMENT_K255:
		return bctoolbox::K25519::ctSize;
	case ZRTP_KEYAGREEMENT_K448:
		return bctoolbox::K448::ctSize;
	case ZRTP_KEYAGREEMENT_K255_KYB512:
		return bctoolbox::K25519::ctSize + bctoolbox::KYBER512::ctSize;
	case ZRTP_KEYAGREEMENT_K255_HQC128:
		return bctoolbox::K25519::ctSize + bctoolbox::HQC128::ctSize;
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
		return bctoolbox::K448::ctSize + bctoolbox::KYBER1024::ctSize;
	case ZRTP_KEYAGREEMENT_K448_HQC256:
		return bctoolbox::K448::ctSize + bctoolbox::HQC256::ctSize;
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
		return bctoolbox::K25519::ctSize + bctoolbox::KYBER512::ctSize + bctoolbox::HQC128::ctSize;
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
		return bctoolbox::K448::ctSize + bctoolbox::KYBER1024::ctSize + bctoolbox::HQC256::ctSize;
	default:
		return 0;
	}
#else /* HAVE_BCTBXPQ */
	return 0;
#endif /* HAVE_BCTBXPQ */
}

uint16_t bzrtp_computeKeyAgreementPublicValueLength(uint8_t keyAgreementAlgo, uint8_t messageType) {
	switch (keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_DH3k	:
		return 384;
	case ZRTP_KEYAGREEMENT_DH2k :
		return 256;
	case ZRTP_KEYAGREEMENT_X255	:
		return 32;
	case ZRTP_KEYAGREEMENT_X448	:
		return 56;
	case ZRTP_KEYAGREEMENT_EC25	:
		return 64;
	case ZRTP_KEYAGREEMENT_EC38	:
		return 96;
	case ZRTP_KEYAGREEMENT_EC52 :
		return 132;
	default :
		// manage all the KEM algo
		switch (messageType) {
		case MSGTYPE_COMMIT: // Commit packet includes the public key
		{
			auto ret = bzrt_getKEMPublicKeyLength(keyAgreementAlgo);
			if (ret%4) { // The size MUST be a multiple of 4 as ZRTP nessage size is given in 4 bytes words not in bytes, it will just be padded at the end with 0 if needed
				ret += 4-ret%4;
			}
			return ret;
		}
		case MSGTYPE_DHPART1: // DHPart1 packet holds the cipher text
		{
			auto ret = bzrt_getKEMCipherTextLength(keyAgreementAlgo);
			if (ret%4) { // The size MUST be a multiple of 4 as ZRTP nessage size is given in 4 bytes words not in bytes, it will just be padded at the end with 0 if needed
				ret += 4-ret%4;
			}
			return ret;
		}
		case MSGTYPE_DHPART2: // DHPart2 holds a nonce - invalid algo would pass if giving this message type
			return ZRTP_KEMPART2_NONCE_SIZE;
		default:
			return 0;
		}
	}
}

uint16_t bzrtp_computeKeyAgreementSharedSecretLength(uint8_t keyAgreementAlgo, uint8_t hashLength) {
	switch (keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_DH3k	:
		return 384;
	case ZRTP_KEYAGREEMENT_DH2k :
		return 256;
	case ZRTP_KEYAGREEMENT_X255	:
		return 32;
	case ZRTP_KEYAGREEMENT_X448	:
		return 56;
	case ZRTP_KEYAGREEMENT_EC25	:
		return 64;
	case ZRTP_KEYAGREEMENT_EC38	:
		return 96;
	case ZRTP_KEYAGREEMENT_EC52 :
		return 132;
#ifdef HAVE_BCTBXPQ
	case ZRTP_KEYAGREEMENT_KYB1:
		return bctoolbox::KYBER512::ssSize;
	case ZRTP_KEYAGREEMENT_KYB2:
		return bctoolbox::KYBER768::ssSize;
	case ZRTP_KEYAGREEMENT_KYB3:
		return bctoolbox::KYBER1024::ssSize;
	case ZRTP_KEYAGREEMENT_HQC1:
		return bctoolbox::HQC128::ssSize;
	case ZRTP_KEYAGREEMENT_HQC2:
		return bctoolbox::HQC192::ssSize;
	case ZRTP_KEYAGREEMENT_HQC3:
		return bctoolbox::HQC256::ssSize;
	case ZRTP_KEYAGREEMENT_K255:
		return bctoolbox::K25519::ssSize;
	case ZRTP_KEYAGREEMENT_K448:
		return bctoolbox::K448::ssSize;
	case ZRTP_KEYAGREEMENT_K255_KYB512:
	case ZRTP_KEYAGREEMENT_K255_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
	case ZRTP_KEYAGREEMENT_K448_HQC256:
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
		return (uint16_t) hashLength;
#endif /* HAVE_BCTBXPQ */
	default:
		return 0;
	}
}

bool_t bzrtp_isKem(uint8_t keyAgreementAlgo) {
	switch (keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_KYB1:
	case ZRTP_KEYAGREEMENT_KYB2:
	case ZRTP_KEYAGREEMENT_KYB3:
	case ZRTP_KEYAGREEMENT_HQC1:
	case ZRTP_KEYAGREEMENT_HQC2:
	case ZRTP_KEYAGREEMENT_HQC3:
	case ZRTP_KEYAGREEMENT_K255:
	case ZRTP_KEYAGREEMENT_K448:
	case ZRTP_KEYAGREEMENT_K255_KYB512:
	case ZRTP_KEYAGREEMENT_K255_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
	case ZRTP_KEYAGREEMENT_K448_HQC256:
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
		return TRUE;
	default:
		return false;
	}
}

uint16_t bzrtp_computeCommitMessageVariableLength(uint8_t keyAgreementAlgo) {
	if (keyAgreementAlgo == ZRTP_KEYAGREEMENT_Prsh) return 24; /* nonce (16 bytes) and keyID(8 bytes) are 24 bytes length in preshared Commit message format */
	if (keyAgreementAlgo == ZRTP_KEYAGREEMENT_Mult) return 16; /* nonce is 16 bytes length in multistream Commit message format */
	if (bzrtp_isKem(keyAgreementAlgo)) {
		auto ret = bzrt_getKEMPublicKeyLength(keyAgreementAlgo);
		if (ret%4) { // The size MUST be a multiple of 4 as ZRTP nessage size is given in 4 bytes words not in bytes, it will just be padded at the end with 0 if needed
			ret += 4-ret%4;
		}
		return 32+ret; // add 32 for the hvi
	}
	switch(keyAgreementAlgo) {
		case ZRTP_KEYAGREEMENT_DH2k :
		case ZRTP_KEYAGREEMENT_X255 :
		case ZRTP_KEYAGREEMENT_X448 :
		case ZRTP_KEYAGREEMENT_EC25 :
		case ZRTP_KEYAGREEMENT_DH3k :
		case ZRTP_KEYAGREEMENT_EC38 :
		case ZRTP_KEYAGREEMENT_EC52 :
			return 32; /* hvi is 32 bytes length in DH Commit message format */
		default:
			return 0; /* return 0 in case of failure: key agreement algo is unknown */
	}
}

/**
 * @brief Convert the bzrtp hash id to its bctoolbox hash id
 * @param[in]	hashAlgo	bzrtp hash id
 * @return	The bctoolbox hash id
 */
static int bzrtp_getHashAlgoId(uint8_t hashAlgo){
	switch(hashAlgo){
	case ZRTP_HASH_S256: return BCTBX_MD_SHA256;
	case ZRTP_HASH_S384: return BCTBX_MD_SHA384;
	case ZRTP_HASH_S512: return BCTBX_MD_SHA512;
	default: return BCTBX_MD_UNDEFINED;
	}
}

#ifdef HAVE_BCTBXPQ
typedef struct bzrtp_KEMContext_struct {
	std::shared_ptr<bctoolbox::KEM> ctx;
	std::vector<uint8_t> publicKey;
	std::vector<uint8_t> secretKey;
	std::vector<uint8_t> sharedSecret;
} bzrtp_KEMContext_t;

/**
 * Create the KEM context
 *
 * @param[in]	keyAgreementAlgo one of ZRTP_KEYAGREEMENT_KYB1, ZRTP_KEYAGREEMENT_KYB2, ZRTP_KEYAGREEMENT_KYB3,
 *										ZRTP_KEYAGREEMENT_HQC1, ZRTP_KEYAGREEMENT_HQC2, ZRTP_KEYAGREEMENT_HQC3,
 *										ZRTP_KEYAGREEMENT_K255, ZRTP_KEYAGREEMENT_K448,
 *										ZRTP_KEYAGREEMENT_K255_KYB512, ZRTP_KEYAGREEMENT_K255_HQC128,
 *										ZRTP_KEYAGREEMENT_K448_KYB1024, ZRTP_KEYAGREEMENT_K448_HQC256
 *										ZRTP_KEYAGREEMENT_K255_KYB512_HQC128, ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256,
 *
 * @return a pointer to the created context, NULL in case of failure
 */
bzrtp_KEMContext_t *bzrtp_createKEMContext(uint8_t keyAgreementAlgo, uint8_t hashAlgo) {
	if (!bzrtp_isKem(keyAgreementAlgo)) {
		return NULL;
	}
	bzrtp_KEMContext_t *context = new bzrtp_KEMContext_t;
	int hashId = bzrtp_getHashAlgoId(hashAlgo);

	switch (keyAgreementAlgo) {
	case ZRTP_KEYAGREEMENT_KYB1:
		context->ctx = std::make_shared<bctoolbox::KYBER512>();
		break;
	case ZRTP_KEYAGREEMENT_KYB2:
		context->ctx = std::make_shared<bctoolbox::KYBER768>();
		break;
	case ZRTP_KEYAGREEMENT_KYB3:
		context->ctx = std::make_shared<bctoolbox::KYBER1024>();
		break;
	case ZRTP_KEYAGREEMENT_HQC1:
		context->ctx = std::make_shared<bctoolbox::HQC128>();
		break;
	case ZRTP_KEYAGREEMENT_HQC2:
		context->ctx = std::make_shared<bctoolbox::HQC192>();
		break;
	case ZRTP_KEYAGREEMENT_HQC3:
		context->ctx = std::make_shared<bctoolbox::HQC256>();
		break;
	case ZRTP_KEYAGREEMENT_K255:
		context->ctx = std::make_shared<bctoolbox::K25519>(hashId);
		break;
	case ZRTP_KEYAGREEMENT_K448:
		context->ctx = std::make_shared<bctoolbox::K448>(hashId);
		break;
	case ZRTP_KEYAGREEMENT_K255_KYB512:
		context->ctx = std::make_shared<bctoolbox::HYBRID_KEM>(std::list<std::shared_ptr<bctoolbox::KEM>>({std::make_shared<bctoolbox::K25519>(hashId), std::make_shared<bctoolbox::KYBER512>()}), hashId);
		break;
	case ZRTP_KEYAGREEMENT_K255_HQC128:
		context->ctx = std::make_shared<bctoolbox::HYBRID_KEM>(std::list<std::shared_ptr<bctoolbox::KEM>>({std::make_shared<bctoolbox::K25519>(hashId), std::make_shared<bctoolbox::HQC128>()}), hashId);
		break;
	case ZRTP_KEYAGREEMENT_K448_KYB1024:
		context->ctx = std::make_shared<bctoolbox::HYBRID_KEM>(std::list<std::shared_ptr<bctoolbox::KEM>>({std::make_shared<bctoolbox::K448>(hashId), std::make_shared<bctoolbox::KYBER1024>()}), hashId);
		break;
	case ZRTP_KEYAGREEMENT_K448_HQC256:
		context->ctx = std::make_shared<bctoolbox::HYBRID_KEM>(std::list<std::shared_ptr<bctoolbox::KEM>>({std::make_shared<bctoolbox::K448>(hashId), std::make_shared<bctoolbox::HQC256>()}), hashId);
		break;
	case ZRTP_KEYAGREEMENT_K255_KYB512_HQC128:
		context->ctx = std::make_shared<bctoolbox::HYBRID_KEM>(std::list<std::shared_ptr<bctoolbox::KEM>>({std::make_shared<bctoolbox::K25519>(hashId), std::make_shared<bctoolbox::KYBER512>(), std::make_shared<bctoolbox::HQC128>()}), hashId);
		break;
	case ZRTP_KEYAGREEMENT_K448_KYB1024_HQC256:
		context->ctx = std::make_shared<bctoolbox::HYBRID_KEM>(std::list<std::shared_ptr<bctoolbox::KEM>>({std::make_shared<bctoolbox::K448>(hashId), std::make_shared<bctoolbox::KYBER1024>(), std::make_shared<bctoolbox::HQC256>()}), hashId);
		break;
	default:
		return NULL;
	}
	return context;
}

/**
 * Generate a key pair and store it in the context
 *
 * @return 0 on success
 */
int bzrtp_KEM_generateKeyPair(bzrtp_KEMContext_t *ctx) {
	if (ctx == NULL || ctx->ctx == NULL) {
		return BZRTP_ERROR_CONTEXTNOTREADY;
	}

	return ctx->ctx->crypto_kem_keypair(ctx->publicKey, ctx->secretKey);
}

/**
 * Extract the public key from context
 * @param[in]	ctx			a valid KEM context
 * @param[out]	publicKey	the key in this context.
 *
 * @return 0 on success
 */
int bzrtp_KEM_getPublicKey(bzrtp_KEMContext_t *ctx, uint8_t *publicKey) {
	if (ctx == NULL || ctx->ctx == NULL) {
		return BZRTP_ERROR_CONTEXTNOTREADY;
	}

	if (ctx->publicKey.size() > 0) {
		memcpy(publicKey,ctx->publicKey.data(), ctx->publicKey.size());
		return 0;
	}
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_KEM_getSharedSecret(bzrtp_KEMContext_t *ctx, uint8_t *sharedSecret) {
	if (ctx == NULL || ctx->ctx == NULL) {
		return BZRTP_ERROR_CONTEXTNOTREADY;
	}

	if (ctx->sharedSecret.size() > 0) {
		memcpy(sharedSecret,ctx->sharedSecret.data(), ctx->sharedSecret.size());
		return 0;
	}
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_KEM_encaps(bzrtp_KEMContext_t *ctx, uint8_t *publicKey, uint8_t *cipherText) {
	if (ctx == NULL || ctx->ctx == NULL) {
		return BZRTP_ERROR_CONTEXTNOTREADY;
	}

	std::vector<uint8_t> ct;
	std::vector<uint8_t> pk(ctx->ctx->get_pkSize());
	memcpy(pk.data(), publicKey, pk.size());

	if (ctx->ctx->crypto_kem_enc(ct, ctx->sharedSecret, pk) == 0) {
		memcpy(cipherText, ct.data(), ct.size());
		return 0;
	}
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_KEM_decaps(bzrtp_KEMContext_t *ctx, uint8_t *cipherText) {
	if (ctx == NULL || ctx->ctx == NULL || ctx->secretKey.size()==0) {
		return BZRTP_ERROR_CONTEXTNOTREADY;
	}
	std::vector<uint8_t> ct(ctx->ctx->get_ctSize());
	memcpy(ct.data(), cipherText, ct.size());
	if (ctx->ctx->crypto_kem_dec(ctx->sharedSecret, ct, ctx->secretKey) == 0) {
		return 0;
	}
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

/**
 * Destroy a KEM context. Safely clean any secrets it may holding
 * @param[in]	ctx				a valid KEM context
 *
 * @return 0 on success
 */
int bzrtp_destroyKEMContext(bzrtp_KEMContext_t *ctx) {
	if (ctx == NULL || ctx->ctx == NULL) {
		return BZRTP_ERROR_CONTEXTNOTREADY;
	}

	ctx->ctx=nullptr;

	bctbx_clean(ctx->secretKey.data(), ctx->secretKey.size());;
	bctbx_clean(ctx->sharedSecret.data(), ctx->sharedSecret.size());;

	delete ctx;
	return 0;
}

#else /* HAVE_BCTBXPQ */
/* KEM functions are implemented in postquantumcryptoengine, stub them if we don't have it */

bzrtp_KEMContext_t *bzrtp_createKEMContext(uint8_t keyAgreementAlgo, uint8_t hashAlgo) {
	return NULL;
}
int bzrtp_KEM_generateKeyPair(bzrtp_KEMContext_t *ctx) {
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_KEM_getPublicKey(bzrtp_KEMContext_t *ctx, uint8_t *publicKey) {
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_KEM_getSharedSecret(bzrtp_KEMContext_t *ctx, uint8_t *sharedSecret) {
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_KEM_encaps(bzrtp_KEMContext_t *ctx, uint8_t *publicKey, uint8_t *cipherText) {
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_KEM_decaps(bzrtp_KEMContext_t *ctx, uint8_t *cipherText) {
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

int bzrtp_destroyKEMContext(bzrtp_KEMContext_t *ctx) {
	return BZRTP_ERROR_CONTEXTNOTREADY;
}

#endif /* HAVE_BCTBXPQ */
