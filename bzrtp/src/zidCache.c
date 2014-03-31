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
#include <string.h>
#include "zidCache.h"

#ifdef HAVE_LIBXML2

#include <libxml/tree.h>
#include <libxml/parser.h>
#include "typedef.h"

#define MIN_VALID_CACHE_LENGTH 56 /* root tag + selfZID tag size */
#define XML_HEADER_STRING "<?xml version='1.0' encoding='utf-8'?>"
#define XML_HEADER_SIZE 38
/* Local functions prototypes */
void bzrtp_strToUint8(uint8_t *outputBytes, uint8_t *inputString, uint16_t inputLength);
void bzrtp_int8ToStr(uint8_t *outputString, uint8_t *inputBytes, uint16_t inputBytesLength);
uint8_t bzrtp_byteToChar(uint8_t inputByte);
uint8_t bzrtp_charToByte(uint8_t inputChar);
void bzrtp_writeCache(bzrtpContext_t *zrtpContext, xmlDocPtr doc);

int bzrtp_getSelfZID(bzrtpContext_t *context, uint8_t selfZID[12]) {
	if (context == NULL) {
		return ZRTP_ZIDCACHE_INVALID_CONTEXT; 
	}
	/* load the cache buffer. TODO: lock it as we may write it */
	if (context->zrtpCallbacks.bzrtp_loadCache != NULL) {
		context->zrtpCallbacks.bzrtp_loadCache(context->channelContext[0]->clientData, &context->cacheBuffer, &(context->cacheBufferLength));
	} else {
		/* we are running cacheless, return a random number */
		bzrtpCrypto_getRandom(context->RNGContext, selfZID, 12);
		return 0; 
	}

	uint8_t *selfZidHex = NULL;
	/* parse the cache to find the ZID element */
	xmlDocPtr doc = NULL;
	if (context->cacheBufferLength>MIN_VALID_CACHE_LENGTH) { /* don't even try to parse it if it is too small, we will create it */
		doc = xmlParseDoc(context->cacheBuffer);
	}
	if (doc != NULL ) { /* there is a cache, try to find our ZID */
		xmlNodePtr cur = xmlDocGetRootElement(doc);
		/* if we found a root element, parse its children node */
		if (cur!=NULL) 
		{
			cur = cur->xmlChildrenNode;
		}
		while (cur!=NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *)"selfZID"))){ /* self ZID found, extract it */
				selfZidHex = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);		
				/* convert it from hexa string to bytes string */
				bzrtp_strToUint8(selfZID, selfZidHex, strlen((char *)selfZidHex));
				break;
			}
			cur = cur->next;
		}
	}


	/* if we didn't found anything in cache, or we have no cache at all: generate ZID, cache string and write it to file */
	if (selfZidHex==NULL) {
		/* generate a random ZID */
		bzrtpCrypto_getRandom(context->RNGContext, selfZID, 12);
		/* convert it to an Hexa String */
		uint8_t newZidHex[25];
		bzrtp_int8ToStr(newZidHex, selfZID, 12);
		newZidHex[24] = '\0'; /* the string must be null terminated for libxml2 to add it correctly in the element */
		xmlFree(doc);
		/* Create a new xml doc */
		doc = xmlNewDoc((const xmlChar *)"1.0");
		/* root tag is "cache" */
		xmlNodePtr rootNode = xmlNewDocNode(doc, NULL, (const xmlChar *)"cache", NULL);
	    xmlDocSetRootElement(doc, rootNode);
		/* add the ZID child */
		xmlNewTextChild(rootNode, NULL, (const xmlChar *)"selfZID", newZidHex);
		
		/* write the cache file and unlock it(TODO)*/
		bzrtp_writeCache(context, doc);
	}
	/* TODO unlock the cache */
	xmlFree(selfZidHex);
	xmlFree(doc);

	return 0;
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
	context->cachedSecret.previouslyVerifiedSas = 0;

	/* parse the cache to find the peer element matching the given ZID */
	xmlDocPtr doc = NULL;
	if (context->cacheBufferLength>MIN_VALID_CACHE_LENGTH) { /* don't even try to parse it if it is too small */
		doc = xmlParseDoc(context->cacheBuffer);
	}
	if (doc != NULL ) { /* there is a cache, try to find our peer element */
		uint8_t peerZidHex[24];
		uint8_t *currentZidHex;
		bzrtp_int8ToStr(peerZidHex, peerZID, 12); /* compute the peerZID as an Hexa string */

		xmlNodePtr cur = xmlDocGetRootElement(doc);
		/* if we found a root element, parse its children node */
		if (cur!=NULL) 
		{
			cur = cur->xmlChildrenNode;
		}
		while (cur!=NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *)"peer"))){ /* found a peer, check his ZID element */
				currentZidHex = xmlNodeListGetString(doc, cur->xmlChildrenNode->xmlChildrenNode, 1); /* ZID is the first element of peer */
				if (memcmp(currentZidHex, peerZidHex, 24) == 0) { /* we found the peer element we are looking for */
					xmlNodePtr peerNode = cur->xmlChildrenNode->next; /* no need to parse the first child as it is the ZID node */
					while (peerNode != NULL) { /* get all the needed information : rs1, rs2, pbx and aux if we found them */
						uint8_t *nodeContent = NULL;
						if (!xmlStrcmp(peerNode->name, (const xmlChar *)"rs1")) {
							nodeContent = xmlNodeListGetString(doc, peerNode->xmlChildrenNode, 1);
							context->cachedSecret.rs1 = (uint8_t *)malloc(RETAINED_SECRET_LENGTH);
							context->cachedSecret.rs1Length = RETAINED_SECRET_LENGTH;
							bzrtp_strToUint8(context->cachedSecret.rs1, nodeContent, 2*RETAINED_SECRET_LENGTH); /* RETAINED_SECRET_LENGTH is in byte, the nodeContent buffer is in hexa string so twice the length of byte string */
						}
						if (!xmlStrcmp(peerNode->name, (const xmlChar *)"rs2")) {
							nodeContent = xmlNodeListGetString(doc, peerNode->xmlChildrenNode, 1);
							context->cachedSecret.rs2 = (uint8_t *)malloc(RETAINED_SECRET_LENGTH);
							context->cachedSecret.rs2Length = RETAINED_SECRET_LENGTH;
							bzrtp_strToUint8(context->cachedSecret.rs2, nodeContent, 2*RETAINED_SECRET_LENGTH); /* RETAINED_SECRET_LENGTH is in byte, the nodeContent buffer is in hexa string so twice the length of byte string */
						}
						if (!xmlStrcmp(peerNode->name, (const xmlChar *)"aux")) {
							nodeContent = xmlNodeListGetString(doc, peerNode->xmlChildrenNode, 1);
							context->cachedSecret.auxsecretLength = strlen((const char *)nodeContent)/2;
							context->cachedSecret.auxsecret = (uint8_t *)malloc(context->cachedSecret.auxsecretLength); /* aux secret is of user defined length, node Content is an hexa string */
							bzrtp_strToUint8(context->cachedSecret.auxsecret, nodeContent, 2*context->cachedSecret.auxsecretLength); 
						}
						if (!xmlStrcmp(peerNode->name, (const xmlChar *)"pbx")) {
							nodeContent = xmlNodeListGetString(doc, peerNode->xmlChildrenNode, 1);
							context->cachedSecret.pbxsecret = (uint8_t *)malloc(RETAINED_SECRET_LENGTH);
							context->cachedSecret.pbxsecretLength = RETAINED_SECRET_LENGTH;
							bzrtp_strToUint8(context->cachedSecret.pbxsecret, nodeContent, 2*RETAINED_SECRET_LENGTH); /* RETAINED_SECRET_LENGTH is in byte, the nodeContent buffer is in hexa string so twice the length of byte string */
						}
						if (!xmlStrcmp(peerNode->name, (const xmlChar *)"pvs")) { /* this one is the previously verified sas flag */
							nodeContent = xmlNodeListGetString(doc, peerNode->xmlChildrenNode, 1);
							if (nodeContent[1] == *"1") { /* pvs is a boolean but is stored as a byte, on 2 hex chars */
								context->cachedSecret.previouslyVerifiedSas = 1;
							}
						}
						xmlFree(nodeContent);
						peerNode = peerNode->next;
					}
					break;
				}
				xmlFree(currentZidHex);
				currentZidHex=NULL;
			}
			cur = cur->next;
		}
	}

	xmlFree(doc);

	return 0;
}

/**
 * @brief Write the given taf into peer Node, if the tag exists, content is replaced
 * Cache file is locked(TODO), read and updated during this call
 *
 * @param[in/out]	context				the current context, used to get the negotiated Hash algorithm and cache access functions and store result
 * @param[in]		peerZID				a byte array of the peer ZID
 * @param[in]		tagName				the tagname of node to be written, it MUST be null terminated
 * @param[in]		tagNameLength		the length of tagname (not including the null termination char)
 * @param[in]		tagContent			the content of the node(a byte buffer which will be converted to hexa string)
 * @param[in]		tagContentLength	the length of the content to be written
 * 
 * return 0 on success, error code otherwise
 */
int bzrtp_writePeerNode(bzrtpContext_t *context, uint8_t peerZID[12], uint8_t *tagName, uint8_t tagNameLength, uint8_t *tagContent, uint32_t tagContentLength) {
	if ((context == NULL) || (context->zrtpCallbacks.bzrtp_loadCache == NULL)) {
		return ZRTP_ZIDCACHE_INVALID_CONTEXT;
	}

	/* turn the tagContent to an hexa string null terminated */
	uint8_t *tagContentHex = (uint8_t *)malloc(2*tagContentLength+1);
	bzrtp_int8ToStr(tagContentHex, tagContent, tagContentLength);
	tagContentHex[2*tagContentLength] = '\0';

	/* reload cache from file locking it (TODO: lock) */
	free(context->cacheBuffer);
	context->cacheBuffer = NULL;
	context->cacheBufferLength = 0;
	context->zrtpCallbacks.bzrtp_loadCache(context->channelContext[0]->clientData, &context->cacheBuffer, &(context->cacheBufferLength));

	/* parse the cache to find the peer element matching the given ZID */
	xmlDocPtr doc = NULL;
	if (context->cacheBufferLength>MIN_VALID_CACHE_LENGTH) { /* don't even try to parse it if it is too small */
		doc = xmlParseDoc(context->cacheBuffer);
	}
	if (doc != NULL ) { /* there is a cache, try to find our peer element */
		uint8_t peerZidHex[25];
		uint8_t *currentZidHex;
		bzrtp_int8ToStr(peerZidHex, peerZID, 12); /* compute the peerZID as an Hexa string */
		peerZidHex[24]='\0';

		xmlNodePtr rootNode = xmlDocGetRootElement(doc);
		xmlNodePtr cur = NULL;
		/* if we found a root element, parse its children node */
		if (rootNode!=NULL) 
		{
			cur = rootNode->xmlChildrenNode->next; /* first node is selfZID, don't parse it */
		}
		uint8_t nodeUpdated = 0; /* a boolean flag set if node is updated */
		while (cur!=NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *)"peer"))){ /* found a peer, check his ZID element */
				currentZidHex = xmlNodeListGetString(doc, cur->xmlChildrenNode->xmlChildrenNode, 1); /* ZID is the first element of peer */
				if (memcmp(currentZidHex, peerZidHex, 24) == 0) { /* we found the peer element we are looking for */
					xmlNodePtr peerNode = cur->xmlChildrenNode->next;
					while (peerNode != NULL && nodeUpdated==0) { /* look for the tag we want to write */
						if ((!xmlStrcmp(peerNode->name, (const xmlChar *)tagName))){ /* check if we already have the tag we want to write */
							xmlNodeSetContent(peerNode, (const xmlChar *)tagContentHex);
							nodeUpdated = 1;
						} else {
							peerNode = peerNode->next;
						}
					}
					if (nodeUpdated == 0) { /* if we didn't found our node, add it at the end of peer node */
						xmlNewTextChild(cur, NULL, (const xmlChar *)tagName, tagContentHex);
						nodeUpdated = 1;
					}
					break;
				}
				xmlFree(currentZidHex);
				currentZidHex=NULL;
			}
			cur = cur->next;
		}

		/* we didn't find the peer element, create it with nodes ZID and tagName */
		if (nodeUpdated == 0) {
			xmlNodePtr peerNode = xmlNewNode(NULL, (const xmlChar *)"peer");
			xmlNewTextChild(peerNode, NULL, (const xmlChar *)"ZID", peerZidHex);
			xmlNewTextChild(peerNode, NULL, (const xmlChar *)tagName, tagContentHex);
			xmlAddChild(rootNode, peerNode);
		}


		/* write the cache file and unlock it(TODO)*/
		bzrtp_writeCache(context, doc);
	}

	xmlFree(doc);
	free(tagContentHex);

	return 0;
}


/*** Local functions implementations ***/

void bzrtp_writeCache(bzrtpContext_t *zrtpContext, xmlDocPtr doc) {
		free(zrtpContext->cacheBuffer);
		zrtpContext->cacheBuffer = NULL;
		zrtpContext->cacheBufferLength = 0;
		xmlChar *xmlStringOutput;
		xmlDocDumpFormatMemoryEnc(doc, &xmlStringOutput, (int *)&zrtpContext->cacheBufferLength, "UTF-8", 0);
		zrtpContext->cacheBuffer = malloc(zrtpContext->cacheBufferLength);
		memcpy(zrtpContext->cacheBuffer, xmlStringOutput, zrtpContext->cacheBufferLength);
		xmlFree(xmlStringOutput);
		zrtpContext->zrtpCallbacks.bzrtp_writeCache(zrtpContext->channelContext[0]->clientData, zrtpContext->cacheBuffer, zrtpContext->cacheBufferLength);
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

#else /* NOT HAVE_LIBXML2 */
int bzrtp_getSelfZID(bzrtpContext_t *context, uint8_t selfZID[12]) {
	if (context == NULL) {
		return ZRTP_ZIDCACHE_INVALID_CONTEXT; 
	}
	/* we are running cacheless, return a random number */
	bzrtpCrypto_getRandom(context->RNGContext, selfZID, 12);
	return 0; 
}
int bzrtp_getPeerAssociatedSecretsHash(bzrtpContext_t *context, uint8_t peerZID[12]) {
		if (context == NULL) {
		return ZRTP_ZIDCACHE_INVALID_CONTEXT;
	}

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
	context->cachedSecret.previouslyVerifiedSas = 0;

	return 0;
}
int bzrtp_writePeerNode(bzrtpContext_t *context, uint8_t peerZID[12], uint8_t *tagName, uint8_t tagNameLength, uint8_t *tagContent, uint32_t tagContentLength) {
	return 0;
}

#endif /* HAVE LIBXML2 */
