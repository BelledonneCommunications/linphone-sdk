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

#endif /* ZIDCACHE_H */
