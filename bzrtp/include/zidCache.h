/**
 @file zidCache.h

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
#ifndef ZIDCACHE_H
#define ZIDCACHE_H

#include "typedef.h"

#define ZRTP_ZIDCACHE_INVALID_CONTEXT	0x2001
#define ZRTP_ZIDCACHE_INVALID_CACHE		0x2002
#define ZRTP_ZIDCACHE_UNABLETOUPDATE	0x2003

/**
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
int bzrtp_getSelfZID(bzrtpContext_t *context, uint8_t selfZID[12]);

/**
 * @brief Parse the cache to find secrets associated to the given ZID, set them and their length in the context if they are found 
 *
 * @param[in/out]	context		the current context, used to get the negotiated Hash algorithm and cache access functions and store result
 * @param[in]		peerZID		a byte array of the peer ZID
 *
 * return 	0 on succes, error code otherwise 
 */
int bzrtp_getPeerAssociatedSecretsHash(bzrtpContext_t *context, uint8_t peerZID[12]);

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
int bzrtp_writePeerNode(bzrtpContext_t *context, uint8_t peerZID[12], uint8_t *tagName, uint8_t tagNameLength, uint8_t *tagContent, uint32_t tagContentLength);
#endif /* ZIDCACHE_H */
