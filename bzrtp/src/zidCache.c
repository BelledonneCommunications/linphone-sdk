/**
 @file zidCache.c

 @brief all ZID and cache related operations are implemented in this file
 - get or create ZID
 - get/update associated secrets
 It supports cacheless implementation when cache file access functions are null

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
#include <stdlib.h>
#include "typedef.h"
#include "string.h"
#include "zidCache.h"

/* local functions prototypes */
int	bzrtp_findNextTag(uint8_t *cacheBuffer, uint32_t cacheBufferLength, uint32_t *position, uint8_t *tagName);
int bzrtp_readTag(uint8_t *cacheBuffer, uint32_t cacheBufferLength, uint32_t *position, uint8_t *tagName, uint8_t tagNameLength, int *dataLength, uint8_t *data);

int bzrtp_createTagFromBytes(uint8_t *tagName, uint8_t tagNameLength, uint8_t *data, uint8_t dataLength, uint8_t *tag);
int	bzrtp_findClosingTag(uint8_t *cacheBuffer, uint32_t cacheBufferLength, uint32_t *position, uint8_t *tagName, uint8_t tagNameLength, uint8_t *data);
void bzrtp_strToUint8(uint8_t *outputBytes, uint8_t *inputString, uint16_t inputLength);
void bzrtp_int8ToStr(uint8_t *outputString, uint8_t *inputBytes, uint16_t inputBytesLength);
uint8_t bzrtp_byteToChar(uint8_t inputByte);
uint8_t bzrtp_charToByte(uint8_t inputChar);

/*
 * @brief : retrieve ZID from cache
 * ZID is randomly generated if cache is empty or inexistant
 * ZID is randamly generated in case of cacheless implementation
 *
 * @param[in] 	context		The current zrpt context, used to access the random function if needed
 * 							and the ZID cache access function
 * @param[out]	selfZID		The ZID, retrieved from cache or randomly generated
 *
 * @return		0 on success
 */
int bzrtp_getSelfZID(bzrtpContext_t *context, uint8_t selfZID[12]) {
	if (context == NULL) {
		return ZRTP_ZIDCACHE_INVALID_CONTEXT; 
	}

	/* we are running cacheless, return a random number */
	if (context->cacheBuffer == NULL) {
		bzrtpCrypto_getRandom(context->RNGContext, selfZID, 12);
		return 0; 
	}

	uint8_t selfZidHex[24]; /* ZID is 96 bytes long -> 24 hexa characters */
	int dataLength = 24;

	uint32_t position=0;
	if (bzrtp_readTag(context->cacheBuffer, context->cacheBufferLength, &position, (uint8_t *)"selfZID", 7, &dataLength, selfZidHex) == 0) { /* we found a selfZID in the cache */
		/* convert it from hexa string to bytes string */
		bzrtp_strToUint8(selfZID, selfZidHex, 24);
	} else { /* no ZID found in the cache, generate it and write it to cache - it will erase anything in the cache which anyway shall be empty */
		bzrtpCrypto_getRandom(context->RNGContext, selfZID, 12); /* generate the ZID */
		uint8_t selfZIDTag[43]; /* tag length is 43 = 9(<selfZID)+24(data length)+ 10(</selfZID>)*/
		int retval = bzrtp_createTagFromBytes((uint8_t *)"selfZID", 7, selfZID, 12, selfZIDTag);
		if (retval!=0) {
			return ZRTP_ZIDCACHE_UNABLETOUPDATE;
		}
		free(context->cacheBuffer);
		context->cacheBufferLength = 43;
		context->cacheBuffer = selfZIDTag;
		context->zrtpCallbacks.bzrtp_writeCache(context->cacheBuffer, context->cacheBufferLength);
	}

	return 0;
}

/* the maximum length of a taf in the ZID cache file */
#define MAX_TAG_LENGTH	64
#define MAX_DATA_LENGTH	2*MAX_AUX_SECRET_LENGTH

/**
 * @brief Read a tag content from the ZID cache file
 *
 * @param[in] 		cacheBuffer			The current cache buffer
 * @param[in] 		cacheBufferLenght	The current cache buffer length
 * @param[in/out] 	position			current position in the cache buffer
 * @param[in]		tagName				The tag name
 * @param[in]		tagNameLength		Length of the tag name
 * @param[in/out]	dataLength			Expected length of data, set to 0 if unknown and then modified to found value
 * @param[out]		data				The data read as a string
 *
 * @return 		0 on succes, -1 if the tag is not found
 */
int bzrtp_readTag(uint8_t *cacheBuffer, uint32_t cacheBufferLength, uint32_t *position, uint8_t *tagName, uint8_t tagNameLength, int *dataLength, uint8_t *data) {
	uint8_t bufferTag[MAX_TAG_LENGTH+3]; /* max tag length + </> */

	int nextTagLength = 0;
	/* find the tag opening */
	while ((nextTagLength = bzrtp_findNextTag(cacheBuffer, cacheBufferLength, position, bufferTag))!=-1) {
		if (memcmp(bufferTag, tagName, tagNameLength) == 0) { /* we found our tag copy it */
			if (*dataLength!=0) { /* we have an expected length, so just read that amount */
				if (*position+*dataLength+tagNameLength+3<=cacheBufferLength) {
					if ((memcmp(cacheBuffer+*position+*dataLength, "</", 2) == 0) && (memcmp(cacheBuffer+*position+*dataLength+2, tagName, tagNameLength)==0) && (cacheBuffer[*position+*dataLength+tagNameLength+2] == *">")) {
						memcpy(data, cacheBuffer+*position, *dataLength);
						*position += *dataLength+tagNameLength+3;
						return 0;
					} else {
						return -1; /* we found the correct tag but the closing wasn't set at the right place*/
					}
				} else {
					return -1;
				}
			} else { /* we have no idea of content lenght, so read everything until we find the closing tag */
				*dataLength = bzrtp_findClosingTag(cacheBuffer, cacheBufferLength, position, tagName, tagNameLength, data);
				if (*dataLength == -1) {
					return -1;
				}
				return 0;
			}
		}
	}
	return -1;
}

/**
 * @brief find the next tag opening
 * at exit, the file stream pointer is positioned at the begining of data inside the tag found
 *
 * @param[in] 		cacheBuffer			The current cache buffer
 * @param[in] 		cacheBufferLenght	The current cache buffer length
 * @param[in/out] 	position			current position in the cache buffer
 * @param[out]		tagName				A buffer to copy the next tag name
 *
 * @result		tagLength on success, -1 if no tag opening were found
 */
int	bzrtp_findNextTag(uint8_t *cacheBuffer, uint32_t cacheBufferLength, uint32_t *position, uint8_t *tagName) {

	/* go to the first taf opening (seach for < not followed by /) */
	while(!((*position>=cacheBufferLength) || ((cacheBuffer[*position] == *"<") && (cacheBuffer[*position+1] != *"/")))) {
		*position +=1;
	}
	
	/* check we are at a tag opening and not end of buffer */
	if (cacheBuffer[*position] == *"<") {
		*position +=1; /* skip the opening < */
		int i=0;
		while (cacheBuffer[*position] != *">") {
			tagName[i++] = cacheBuffer[*position];
			*position += 1;
		}
		*position +=1; /* set the position just after the tag closing >*/
		return i;
	}

	return -1;
}


/**
 * @brief	Get a data buffer and convert it to hexa string and add opening and closing tags
 *
 * @param[in]	tagName 			a string containing the tag name
 * @param[in]	tagNameLength		the tag name length in chars
 * @param[in]	data				the byte buffer to be converted to hex and put into the tag
 * @param[in]	dataLength			the data buffer length in bytes
 * @param[out]	tag					the output buffer, length must be at least 2*tagNameLength+5+2*dataLength
 *
 * @return		0 on success
 */

int bzrtp_createTagFromBytes(uint8_t *tagName, uint8_t tagNameLength, uint8_t *data, uint8_t dataLength, uint8_t *tag) {
	memcpy(tag, "<", 1);
	tag++;
	memcpy(tag, tagName, tagNameLength);
	tag+=tagNameLength;
	memcpy(tag, ">", 1);
	tag++;
	bzrtp_int8ToStr(tag, data, dataLength);
	tag+=2*dataLength;
	memcpy(tag, "</", 2);
	tag+=2;
	memcpy(tag, tagName, tagNameLength);
	tag+=tagNameLength;
	memcpy(tag, ">", 1);
	tag++;
	
	return 0;
}



/**
 * @brief Convert an hexadecimal string into the corresponding byte buffer
 *
 * @param[out]	outputBytes			The output bytes buffer, must have a length of half the input string buffer
 * @param[in]	inputString			The input string buffer, must be hexadecimal(it is not checked by function, any non hexa char is converted to 0)
 * @param[in]	inputStringLength	The lenght in chars of the string buffer, output is half this length
 */
void bzrtp_strToUint8(uint8_t *outputBytes, uint8_t *inputString, uint16_t inputStringLength) {
	int i;
	for (i=0; i<inputStringLength/2; i++) {
		outputBytes[i] = (bzrtp_charToByte(inputString[2*i]))<<4 | bzrtp_charToByte(inputString[2*i+1]);
	}
}

/**
 * @brief Convert a byte buffer into the corresponding hexadecimal string
 *
 * @param[out]	outputString		The output string buffer, must have a length of twice the input bytes buffer
 * @param[in]	inputBytes			The input bytes buffer
 * @param[in]	inputBytesLength	The lenght in bytes buffer, output is twice this length
 */
void bzrtp_int8ToStr(uint8_t *outputString, uint8_t *inputBytes, uint16_t inputBytesLength) {
	int i;
	for (i=0; i<inputBytesLength; i++) {
		outputString[2*i] = bzrtp_byteToChar((inputBytes[i]>>4)&0x0F);
		outputString[2*i+1] = bzrtp_byteToChar(inputBytes[i]&0x0F);
	}
}


/**
 * @brief	convert an hexa char [0-9a-fA-F] into the corresponding unsigned integer value
 * Any invalid char will be converted to zero without any warning
 *
 * @param[in]	inputChar	a char which shall be in range [0-9a-fA-F]
 *
 * @return		the unsigned integer value in range [0-15]
 */
uint8_t bzrtp_charToByte(uint8_t inputChar) {
	/* 0-9 */
	if (inputChar>0x29 && inputChar<0x3A) {
		return inputChar - 0x30;
	}

	/* a-f */
	if (inputChar>0x60 && inputChar<0x67) {
		return inputChar - 0x57; /* 0x57 = 0x61(a) + 0x0A*/
	}

	/* A-F */
	if (inputChar>0x40 && inputChar<0x47) {
		return inputChar - 0x37; /* 0x37 = 0x41(a) + 0x0A*/
	}

	/* shall never arrive here, string is not Hex*/
	return 0;

}

/**
 * @brief	convert a byte which value is in range [0-15] into an hexa char [0-9a-fA-F]
 *
 * @param[in]	inputByte	an integer which shall be in range [0-15]
 *
 * @return		the hexa char [0-9a-f] corresponding to the input
 */
uint8_t bzrtp_byteToChar(uint8_t inputByte) {
	inputByte &=0x0F; /* restrict the input value to range [0-15] */
	/* 0-9 */
	if(inputByte<0x0A) {
		return inputByte+0x30;
	}
	/* a-f */
	return inputByte + 0x57;
}

/**
 * @brief find the closing tag and copy in a buffer everything we find before arriving to it
 * at exit, the cacheBuffer is positioned at the end of the closing tag
 *
 * @param[in] 		cacheBuffer			The current cache buffer
 * @param[in] 		cacheBufferLenght	The current cache buffer length
 * @param[in/out] 	position			current position in the cache buffer
 * @param[in]		tagName				The tag name to to find
 * @param[in]		tagNameLength		The tag name length in bytes
 * @param[out]		data				An output buffer
 *
 * @result		dataLength on success, -1 if no tag closing were found
 */
int	bzrtp_findClosingTag(uint8_t *cacheBuffer, uint32_t cacheBufferLength, uint32_t *position, uint8_t *tagName, uint8_t tagNameLength, uint8_t *data) {
	int dataLength = 0;
	/* go to the next closing tag (seach for </) */
	while (*position+tagNameLength+2<cacheBufferLength) {
		if ((cacheBuffer[*position] == *"<") && (cacheBuffer[*position+1] == *"/")) { /* found a closing tag */
			/* it is the good one */
			if ((memcmp(cacheBuffer+*position+2, tagName, tagNameLength) == 0) && (cacheBuffer[*position+tagNameLength+2] == *">")) {
				*position += tagNameLength+3;
				return dataLength;
			} else { /* this is not the closing tag you are looking for */
				data[dataLength++] = *"<";
				data[dataLength++] = *"/";
				*position += 2;
			}
		} else { /* no tag found, just copy data into the output buffer and increase position */
			data[dataLength++] = cacheBuffer[*position];
			*position +=1;
		}
	}
	return -1;
}


/**
 * @brief Parse the cache to find secrets associated to the given ZID, set them and their length in the context if they are found 
 *
 * @param[in/out]	context		the current context, used to get the negotiated Hash algorithm and cache access functions and store result
 * @param[in]		peerZID		a byte array of the peer ZID
 *
 * return 	0 on succes, error code otherwise 
 */
int bzrtp_getPeerAssociatedSecretsHash(bzrtpContext_t *context, uint8_t peerZID[12]) {
	if (context == NULL) {
		return ZRTP_ZIDCACHE_INVALID_CONTEXT;
	}
	uint8_t bufferTag[MAX_TAG_LENGTH+3]; /* max tag length + </> */
	uint8_t bufferData[4*MAX_DATA_LENGTH+8*MAX_TAG_LENGTH];

	/* resert cached secret buffer */
	free(context->cachedSecret.rs1);
	free(context->cachedSecret.rs2);
	free(context->cachedSecret.pbxsecret);
	free(context->cachedSecret.auxsecret);
	context->cachedSecret.rs1 = NULL;
	context->cachedSecret.rs1Length = 0;
	context->cachedSecret.rs2 = NULL;
	context->cachedSecret.rs2Length = 0;
	context->cachedSecret.pbxsecret = NULL;
	context->cachedSecret.pbxsecretLength = 0;
	context->cachedSecret.auxsecret = NULL;
	context->cachedSecret.auxsecretLength = 0;


	/* get the peer tag matching the given ZID, form is
	 * <peer>
	 * 		<ZID>data</ZID> (only this one is mandatory, data is 24 hexa char)
	 * 		<rs1>data</rs1> (data length depends on negotiated HMAC algorithm)
	 * 		<rs2>data</rs1> (data length depends on negotiated HMAC algorithm)
	 * 		<pbx>data</pbx> (data length depends on negotiated HMAC algorithm)
	 * 		<aux>data</aux> (data length is variable)
	 * 	</peer>
	 * WARNING: if we want to add tags in the peer one, make them 3 chars long too
	 */

	/* do we have any cache loaded or we are running cacheless */
	if (context->cacheBuffer != NULL && context->cacheBufferLength>43) { /* first 43 bytes in the cache must be the self ZID, no need to parse the cache if we have just this */
		uint8_t *cacheBuffer = context->cacheBuffer; /* just to get code easier to read */
		uint32_t cacheBufferLength = context->cacheBufferLength;  /* just to get code easier to read */
		uint32_t position = 43; /* start position in the cache buffer, set it after the self ZID */

		/* convert peerZID to a tag string */
		uint8_t peerZIDtag[35]; /* ZID tag which is mandatory and the first one in the peer length is 5 + 24 + 6 = 35 */
		bzrtp_createTagFromBytes((uint8_t *)"ZID", 3, peerZID, 12, peerZIDtag);

		/* get the next peer tag */
		while (bzrtp_findNextTag(cacheBuffer, cacheBufferLength, &position, bufferTag)!=-1) {
			if (memcmp(bufferTag, "peer", 4) == 0) { /* we found a peer tag */
				if (memcmp(cacheBuffer+position, peerZIDtag, 35) == 0) { /* this is the ZID we are looking for */
					/* get all the data up to the peer closing tag, buffer data shall contain any of the rs1, rs2, pbx or aux tag */
					int peerDataLength = bzrtp_findClosingTag(cacheBuffer, cacheBufferLength, &position, (uint8_t *)"peer", 4, bufferData);
					if	(peerDataLength > 0) {
						/* get the first tag in the buffer */
						uint16_t index = 0;

						while(bufferData[index] == *"<" && bufferData[index+4] == *">") {
							memcpy(bufferTag, bufferData+index+1, 3); /* store the found tag name */
							index+=5;
							/*get the tag content in secretDataBuffer */
							uint8_t secretDataBuffer[MAX_DATA_LENGTH]; /* this one store the hexa string */
							int i = 0;
							while(bufferData[index] != *"<" && index<peerDataLength) {
								secretDataBuffer[i++] = bufferData[index++];
							}

							/* check we have the correct closing tag */
							if ((bufferData[index+1] == *"/") && (memcmp(bufferData+2+index, bufferTag, 3) == 0)) {
								if (memcmp(bufferTag, "rs1", 3) == 0) { /* found a rs1 tag */
									context->cachedSecret.rs1 = (uint8_t *)malloc(i/2*sizeof(uint8_t));
									bzrtp_strToUint8(context->cachedSecret.rs1, secretDataBuffer, i);
									context->cachedSecret.rs1Length = i/2;
								} else if (memcmp(bufferTag, "rs2", 3) == 0) { /* found a rs2 tag */
									context->cachedSecret.rs2 = (uint8_t *)malloc(i/2*sizeof(uint8_t));
									bzrtp_strToUint8(context->cachedSecret.rs2, secretDataBuffer, i);
									context->cachedSecret.rs2Length = i/2;
								} else if (memcmp(bufferTag, "pbx", 3) == 0) { /* found a pbx tag */
									context->cachedSecret.pbxsecret = (uint8_t *)malloc(i/2*sizeof(uint8_t));
									bzrtp_strToUint8(context->cachedSecret.pbxsecret, secretDataBuffer, i);
									context->cachedSecret.pbxsecretLength = i/2;
								} else if (memcmp(bufferTag, "aux", 3) == 0) { /* found a aux tag */
									context->cachedSecret.auxsecret = (uint8_t *)malloc(i/2*sizeof(uint8_t));
									bzrtp_strToUint8(context->cachedSecret.auxsecret, secretDataBuffer, i);
									context->cachedSecret.auxsecretLength = i/2;
								}
								index+=6; /* move the index after the closing tag */
							} else { /*we have a incorrect xml if we arrive here, the tag contains not only data but also an other tag... */
								index--;
							}
						}
					
					}
				}
			}
		}
	}

	return 0;
}
