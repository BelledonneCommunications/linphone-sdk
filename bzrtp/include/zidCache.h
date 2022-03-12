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
#ifndef ZIDCACHE_H
#define ZIDCACHE_H

#include "typedef.h"

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief Parse the cache to find secrets associated to the given ZID, set them and their length in the context if they are found 
 *		Note: this function also retrieve zuid(set in the context) wich allow successive calls to cache operation to be faster.
 *
 * @param[in,out]	context		the current context, used to get the cache db pointer, self and peer URI and store results
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
 * @param[in,out]	dbPointer	the opened sqlite database pointer
 * @param[in]		selfURI		local URI, must be already associated to a ZID in the DB(association is performed by any call of getSelfZID on this URI)
 * @param[in]		peerURI		peer URI
 * @param[in]		peerZID		peer ZID
 * @param[in]		insertFlag	A boolean managing insertion or not of a new row:
 * 					- BZRTP_ZIDCACHE_DONT_INSERT_ZUID : if not found identity binding won't lead to insertion and return zuid will be 0
 * 					- BZRTP_ZIDCACHE_INSERT_ZUID : if not found, insert a new row in ziduri table and return newly inserted zuid
 * @param[out]		zuid		the internal db reference to the data row matching this particular pair of correspondant
 * 					if identity binding is not found and insertFlag set to 0, this value is set to 0
 * @param[in]		zidCacheMutex	Points to a mutex used to lock zidCache database access, ignored if NULL
 *
 * @return 0 on success, BZRTP_ERROR_CACHE_PEERNOTFOUND if peer was not in and the insert flag is not set to BZRTP_ZIDCACHE_INSERT_ZUID, error code otherwise
 */
#define BZRTP_ZIDCACHE_DONT_INSERT_ZUID	0
#define BZRTP_ZIDCACHE_INSERT_ZUID	1
BZRTP_EXPORT int bzrtp_cache_getZuid(void *dbPointer, const char *selfURI, const char *peerURI, const uint8_t peerZID[12], const uint8_t insertFlag, int *zuid, bctbx_mutex_t *zidCacheMutex);

/**
 * @brief This is a convenience wrapper to the bzrtp_cache_write function which will also take care of
 *        setting the ziduri table 'active' flag to one for the current row and reset all other rows with matching peeruri
 *
 * Write(insert or update) data in cache, adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be inserted, lengths of theses values
 *		All three arrays must be the same lenght: columnsCount
 * 		If the row isn't present in the given table, it will be inserted
 *
 * @param[in,out]	context		the current context, used to get the cache db pointer, zuid and cache mutex
 * @param[in]		tableName	The name of the table to write in the db, must already exists. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to update
 * @param[in]		values		An array of buffers containing the values to insert/update matching the order of columns array
 * @param[in]		lengths		An array of integer containing the lengths of values array buffer matching the order of columns array
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 *
 * @return 0 on succes, error code otherwise
 */
BZRTP_EXPORT int bzrtp_cache_write_active(bzrtpContext_t *context, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount);

#ifdef __cplusplus
}
#endif

#endif /* ZIDCACHE_H */
