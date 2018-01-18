/**
 @file zidCache.h

 @brief all ZID and cache related operations are implemented in this file
 - get or create ZID
 - get/update associated secrets
 It supports cacheless implementation when cache file access functions are null

 @author Johan Pascal

 @copyright Copyright (C) 2017 Belledonne Communications, Grenoble, France
 
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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef ZIDCACHE_H
#define ZIDCACHE_H

#include "typedef.h"

#define BZRTP_ZIDCACHE_INVALID_CONTEXT	0x2001
#define BZRTP_ZIDCACHE_INVALID_CACHE	0x2002
#define BZRTP_ZIDCACHE_UNABLETOUPDATE	0x2003
#define BZRTP_ZIDCACHE_UNABLETOREAD	0x2004
#define BZRTP_ZIDCACHE_BADINPUTDATA	0x2005
#define BZRTP_ZIDCACHE_RUNTIME_CACHELESS	0x2010

/**
 * @brief Parse the cache to find secrets associated to the given ZID, set them and their length in the context if they are found 
 *		Note: this function also retrieve zuid(set in the context) wich allow successive calls to cache operation to be faster.
 *
 * @param[in/out]	context		the current context, used to get the cache db pointer, self and peer URI and store results
 * @param[in]		peerZID		a byte array of the peer ZID
 *
 * return 	0 on succes, error code otherwise 
 */
BZRTP_EXPORT int bzrtp_getPeerAssociatedSecrets(bzrtpContext_t *context, uint8_t peerZID[12]);

/**
 * @brief get the cache internal id used to bind local uri(hence local ZID associated to it)<->peer uri/peer ZID.
 *	Providing a valid local URI(already present in cache), a peer ZID and peer URI will return the zuid creating it if needed and requested
 *	Any pair ZID/sipURI shall identify an account on a device.
 *
 * @param[in/out]	db		the opened sqlite database pointer
 * @param[in]		selfURI		local URI, must be already associated to a ZID in the DB(association is performed by any call of getSelfZID on this URI)
 * @param[in]		peerURI		peer URI
 * @param[in]		peerZID		peer ZID
 * @param[in]		insertFlag	A boolean managing insertion or not of a new row:
 * 					- BZRTP_ZIDCACHE_DONT_INSERT_ZUID : if not found identity binding won't lead to insertion and return zuid will be 0
 * 					- BZRTP_ZIDCACHE_INSERT_ZUID : if not found, insert a new row in ziduri table and return newly inserted zuid
 * @param[out]		zuid		the internal db reference to the data row matching this particular pair of correspondant
 * 					if identity binding is not found and insertFlag set to 0, this value is set to 0
 *
 * @return 0 on success, BZRTP_ERROR_CACHE_PEERNOTFOUND if peer was not in and the insert flag is not set to BZRTP_ZIDCACHE_INSERT_ZUID, error code otherwise
 */
#define BZRTP_ZIDCACHE_DONT_INSERT_ZUID	0
#define BZRTP_ZIDCACHE_INSERT_ZUID	1
BZRTP_EXPORT int bzrtp_cache_getZuid(void *dbPointer, const char *selfURI, const char *peerURI, const uint8_t peerZID[12], const uint8_t insertFlag, int *zuid);

#endif /* ZIDCACHE_H */
