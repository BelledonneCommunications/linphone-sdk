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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <stdlib.h>
#include <string.h>
#include "typedef.h"
#include <bctoolbox/crypto.h>
#include "cryptoUtils.h"
#include "zidCache.h"

#ifdef ZIDCACHE_ENABLED
#include "sqlite3.h"

/* define a version number for the DB schema as an interger MMmmpp */
/* current version is 0.0.1 */
#define ZIDCACHE_DBSCHEMA_VERSION_NUMBER 000001

static int callback_getSelfZID(void *data, int argc, char **argv, char **colName){
	uint8_t **selfZID = (uint8_t **)data;

	/* we selected zid only, it must then be in argv[0] */
	if (argv[0]) {
		*selfZID = (uint8_t *)malloc(12*sizeof(uint8_t));
		memcpy(*selfZID, argv[0], 12);
	}

	return 0;
}

static int callback_getUserVersion(void *data, int argc, char **argv, char **colName){
	int *userVersion = (int *)data;

	if (argv[0]) {
		*userVersion = atoi(argv[0]);
	}

	return 0;
}

/* ZID cache is split in several tables
 * ziduri : zuid(unique key) | ZID | selfuri | peeruri
 *         zuid(ZID/URI binding id) will be used for fastest access to the cache, it binds a local user(self uri/self ZID) to a peer identified both by URI and ZID 
 *         self ZID is stored in this table too in a record having 'self' as peer uri, each local user(uri) has a different ZID 
 *
 * All values except zuid in the following tables are blob, actual integers are split and stored in big endian by callers
 * zrtp : zuid(as foreign key) | rs1 | rs2 | aux secret | pbx secret | pvs flag
 * lime : zuid(as foreign key) | sndKey | rcvKey | sndSId | rcvSId | snd Index | rcv Index | valid
 */
int bzrtp_initCache(void *dbPointer) {
	char* errmsg=NULL;
	int ret;
	char *sql;
	sqlite3_stmt *stmt = NULL;
	int userVersion=-1;
	sqlite3 *db = (sqlite3 *)dbPointer;
	int retval = 0;

	if (dbPointer == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	/* get current db schema version (user_version pragma in sqlite )*/
	sql = sqlite3_mprintf("PRAGMA user_version;");
	ret = sqlite3_exec(db, sql, callback_getUserVersion, &userVersion, &errmsg);
	if (ret!=SQLITE_OK) {
		sqlite3_free(errmsg);
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	/* Here check version number and provide upgrade is needed */
	if (userVersion != ZIDCACHE_DBSCHEMA_VERSION_NUMBER) {
		if (userVersion > ZIDCACHE_DBSCHEMA_VERSION_NUMBER) { /* nothing to do if we encounter a superior version number than expected, just hope it is compatible */
			//TODO: Log this event
		} else { /* Perform update if needed */
			/* update the schema version in DB metadata */
			sql = sqlite3_mprintf("PRAGMA user_version = %d;",ZIDCACHE_DBSCHEMA_VERSION_NUMBER);
			ret = sqlite3_prepare(db, sql, -1, &stmt, NULL);
			if (ret != SQLITE_OK) {
				return BZRTP_ZIDCACHE_UNABLETOUPDATE;
			}
			ret = sqlite3_step(stmt);
			if (ret != SQLITE_DONE) {
				return BZRTP_ZIDCACHE_UNABLETOUPDATE;
			}
			sqlite3_finalize(stmt);

			/* setup return value : SETUP for a brand new populated cache, update if we updated the scheme */
			if (userVersion == 0) {
				retval = BZRTP_CACHE_SETUP;
			} else {
				retval = BZRTP_CACHE_UPDATE;
			}
		}
	}

	/* make sure foreign key are turned ON on this connection */
	ret = sqlite3_prepare(db, "PRAGMA foreign_keys = ON;", -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}
	ret = sqlite3_step(stmt);
	if (ret != SQLITE_DONE) {
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}
	sqlite3_finalize(stmt);

	/* check/create the ziduri table */
	ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS ziduri ("
							"zuid          INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
							"zid		BLOB NOT NULL DEFAULT '000000000000',"
							"selfuri	 TEXT NOT NULL DEFAULT 'unset',"
							"peeruri	 TEXT NOT NULL DEFAULT 'unset'"
						");",
			0,0,&errmsg);
	if(ret != SQLITE_OK) {
		sqlite3_free(errmsg);
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}

	/* check/create the zrtp table */
	ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS zrtp ("
							"zuid		INTEGER NOT NULL DEFAULT 0 UNIQUE,"
							"rs1		BLOB DEFAULT NULL,"
							"rs2		BLOB DEFAULT NULL,"
							"aux		BLOB DEFAULT NULL,"
							"pbx		BLOB DEFAULT NULL,"
							"pvs		BLOB DEFAULT NULL,"
							"FOREIGN KEY(zuid) REFERENCES ziduri(zuid) ON UPDATE CASCADE ON DELETE CASCADE"

						");",
			0,0,&errmsg);
	if(ret != SQLITE_OK) {
		sqlite3_free(errmsg);
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}

	/* check/create the lime table */
	ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS lime ("
							"zuid		INTEGER NOT NULL DEFAULT 0 UNIQUE,"
							"sndKey		BLOB DEFAULT NULL,"
							"rcvKey		BLOB DEFAULT NULL,"
							"sndSId		BLOB DEFAULT NULL,"
							"rcvSId		BLOB DEFAULT NULL,"
							"sndIndex	BLOB DEFAULT NULL,"
							"rcvIndex	BLOB DEFAULT NULL,"
							"valid		BLOB DEFAULT NULL,"
							"FOREIGN KEY(zuid) REFERENCES ziduri(zuid) ON UPDATE CASCADE ON DELETE CASCADE"

						");",
			0,0,&errmsg);
	if(ret != SQLITE_OK) {
		sqlite3_free(errmsg);
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}


	return retval;
}

int bzrtp_getSelfZID(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext) {
	char* errmsg=NULL;
	int ret;
	char *stmt;
	uint8_t *localZID = NULL;
	sqlite3 *db = (sqlite3 *)dbPointer;

	if (dbPointer == NULL) { /* we are running cacheless, generate a random ZID if we have a RNG*/
		if (RNGContext != NULL) {
			bctbx_rng_get(RNGContext, selfZID, 12);
			return 0;
		} else {
			return BZRTP_CACHE_DATA_NOTFOUND;
		}
	}

	/* check/create the self zid in ziduri table, ORDER BY is just to ensure consistent return in case of inconsistent table(with several self ZID for the same selfuri) */
	stmt = sqlite3_mprintf("SELECT zid FROM ziduri WHERE selfuri=%Q AND peeruri='self' ORDER BY zuid LIMIT 1;",selfURI);
	ret = sqlite3_exec(db,stmt,callback_getSelfZID,&localZID,&errmsg);
	if (ret != SQLITE_OK) {
		sqlite3_free(errmsg);
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}
	sqlite3_free(stmt);

	/* Do we have a self ZID in cache? */
	if (localZID == NULL) {
		uint8_t generatedZID[12];
		sqlite3_stmt *insertStatement = NULL;

		/* generate a ZID if we can */
		if (RNGContext != NULL) {
			bctbx_rng_get(RNGContext, generatedZID, 12);
		} else {
			return BZRTP_CACHE_DATA_NOTFOUND;
		}

		/* insert it in the table */
		ret = sqlite3_prepare_v2(db, "INSERT INTO ziduri (zid,selfuri,peeruri) VALUES(?,?,?);", -1, &insertStatement, NULL);
		if (ret != SQLITE_OK) {
			return BZRTP_ZIDCACHE_UNABLETOUPDATE;
		}
		sqlite3_bind_blob(insertStatement, 1, generatedZID, 12, SQLITE_TRANSIENT);
		sqlite3_bind_text(insertStatement, 2, selfURI,-1,SQLITE_TRANSIENT);
		sqlite3_bind_text(insertStatement, 3, "self",-1,SQLITE_TRANSIENT);

		ret = sqlite3_step(insertStatement);
		if (ret!=SQLITE_DONE) {
			return BZRTP_ZIDCACHE_UNABLETOUPDATE;
		}
		sqlite3_finalize(insertStatement);

		/* copy it in the output buffer */
		memcpy(selfZID, generatedZID,12);
	} else { /* we found a ZID, copy it in the output buffer */
		memcpy(selfZID, localZID, 12);
		free(localZID);
	}

	return 0;
}

/**
 * @brief Parse the cache to find secrets associated to the given ZID, set them and their length in the context if they are found 
 *
 * @param[in/out]	context		the current context, used to get the negotiated Hash algorithm, cache db, peerURI and store results
 * @param[in]		peerZID		a byte array of the peer ZID
 *
 * return 	0 on succes, error code otherwise 
 */
int bzrtp_getPeerAssociatedSecrets(bzrtpContext_t *context, uint8_t peerZID[12]) {
	char *stmt = NULL;
	int ret;
	sqlite3_stmt *sqlStmt = NULL;
	int length =0;

	if (context == NULL) {
		return BZRTP_ZIDCACHE_INVALID_CONTEXT;
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

	/* are we going cacheless at runtime */
	if (context->zidCache == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	/* get all secrets from zrtp table, ORDER BY is just to ensure consistent return in case of inconsistent table) */
	stmt = sqlite3_mprintf("SELECT z.zuid, z.rs1, z.rs2, z.aux, z.pbx, z.pvs FROM ziduri as zu LEFT JOIN zrtp as z ON z.zuid=zu.zuid WHERE zu.selfuri=? AND zu.peeruri=? AND zu.zid=? ORDER BY zu.zuid LIMIT 1;");
	ret = sqlite3_prepare_v2(context->zidCache, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}
	sqlite3_bind_text(sqlStmt, 1, context->selfURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_text(sqlStmt, 2, context->peerURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_blob(sqlStmt, 3, peerZID, 12, SQLITE_TRANSIENT);

	ret = sqlite3_step(sqlStmt);

	if (ret!=SQLITE_ROW) {
		sqlite3_finalize(sqlStmt);
		if (ret == SQLITE_DONE) {/* not found in cache, just leave cached secrets reset, but retrieve (or create) zuid */
			return bzrtp_cache_getZuid((void *)context->zidCache, context->selfURI, context->peerURI, context->peerZID, &context->zuid);
		} else { /* we had an error querying the DB... */
			return BZRTP_ZIDCACHE_UNABLETOREAD;
		}
	}

	/* get zuid from column 0 */
	context->zuid = sqlite3_column_int(sqlStmt, 0);

	/* retrieve values : rs1, rs2, aux, pbx, they all are blob, columns 1,2,3,4 */
	length = sqlite3_column_bytes(sqlStmt, 1);
	if (length>0) { /* we have rs1 */
		context->cachedSecret.rs1Length = length;
		context->cachedSecret.rs1 = (uint8_t *)malloc(length*sizeof(uint8_t));
		memcpy(context->cachedSecret.rs1, sqlite3_column_blob(sqlStmt, 1), length);
	}

	length = sqlite3_column_bytes(sqlStmt, 2);
	if (length>0) { /* we have rs2 */
		context->cachedSecret.rs2Length = length;
		context->cachedSecret.rs2 = (uint8_t *)malloc(length*sizeof(uint8_t));
		memcpy(context->cachedSecret.rs2, sqlite3_column_blob(sqlStmt, 2), length);
	}

	length = sqlite3_column_bytes(sqlStmt, 3);
	if (length>0) { /* we have aux */
		context->cachedSecret.auxsecretLength = length;
		context->cachedSecret.auxsecret = (uint8_t *)malloc(length*sizeof(uint8_t));
		memcpy(context->cachedSecret.auxsecret, sqlite3_column_blob(sqlStmt, 3), length);
	}

	length = sqlite3_column_bytes(sqlStmt, 4);
	if (length>0) { /* we have pbx */
		context->cachedSecret.pbxsecretLength = length;
		context->cachedSecret.pbxsecret = (uint8_t *)malloc(length*sizeof(uint8_t));
		memcpy(context->cachedSecret.pbxsecret, sqlite3_column_blob(sqlStmt, 4), length);
	}

	/* pvs is stored as blob in memory, just get the first byte(length shall be one anyway) and consider */
	/* it may be NULL -> consider it 0 */
	length = sqlite3_column_bytes(sqlStmt, 5);
	if (length!=1) {
		context->cachedSecret.previouslyVerifiedSas = 0; /* anything wich is not 0x01 is considered 0, so none or more than 1 byte is 0 */
	} else {
		if (*((uint8_t *)sqlite3_column_blob(sqlStmt, 5)) == 0x01) {
			context->cachedSecret.previouslyVerifiedSas = 1;
		} else {
			context->cachedSecret.previouslyVerifiedSas = 0; /* anything wich is not 0x01 is considered 0 */
		}
	}

	sqlite3_finalize(sqlStmt);
	return 0;
}

/**
 * @brief get the cache internal id used to bind local uri(hence local ZID associated to it)<->peer uri/peer ZID.
 *	Providing a valid local URI(already present in cache), a peer ZID and peer URI will return the zuid creating it if needed
 *	Any pair ZID/sipURI shall identify an account on a device.
 *
 * @param[in/out]	db		the opened sqlite database pointer
 * @param[in]		selfURI		local URI, must be already associated to a ZID in the DB(association is performed by any call of getSelfZID on this URI)
 * @param[in]		peerURI		peer URI
 * @param[in]		peerZID		peer ZID
 * @param[out]		zuid		the internal db reference to the data row matching this particular pair of correspondant
 *
 * @return 0 on success, error code otherwise
 */
int bzrtp_cache_getZuid(void *dbPointer, const char *selfURI, const char *peerURI, const uint8_t peerZID[12], int *zuid) {
	char *stmt=NULL;
	int ret;
	sqlite3_stmt *sqlStmt = NULL;
	sqlite3 *db = (sqlite3 *)dbPointer;

	if (dbPointer == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	/* Try to fetch the requested zuid */
	stmt = sqlite3_mprintf("SELECT zuid FROM ziduri WHERE selfuri=? AND peeruri=? AND zid=? ORDER BY zuid LIMIT 1;");
	ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	sqlite3_bind_text(sqlStmt, 1, selfURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_text(sqlStmt, 2, peerURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_blob(sqlStmt, 3, peerZID, 12, SQLITE_TRANSIENT);

	ret = sqlite3_step(sqlStmt);

	if (ret!=SQLITE_ROW) { /* We didn't found this binding in the DB */
		sqlite3_finalize(sqlStmt);
		if (ret == SQLITE_DONE) { /* query executed correctly, just our data is not there */
			uint8_t *localZID = NULL;
			char *errmsg=NULL;

			/* check that we have a self ZID matching the self URI and insert a new row */
			stmt = sqlite3_mprintf("SELECT zid FROM ziduri WHERE selfuri=%Q AND peeruri='self' ORDER BY zuid LIMIT 1;",selfURI);
			ret = sqlite3_exec(db,stmt,callback_getSelfZID,&localZID,&errmsg);
			sqlite3_free(stmt);
			if (ret != SQLITE_OK) {
				sqlite3_free(errmsg);
				return BZRTP_ZIDCACHE_UNABLETOREAD;
			}

			if (localZID==NULL) { /* this sip URI is not in our DB, do not create an association with the peer ZID/URI binding */
				return BZRTP_ZIDCACHE_BADINPUTDATA;
			} else { /* yes we know this URI on local device, add a row in the ziduri table */
				stmt = sqlite3_mprintf("INSERT INTO ziduri (zid,selfuri,peeruri) VALUES(?,?,?);");
				ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
				if (ret != SQLITE_OK) {
					return BZRTP_ZIDCACHE_UNABLETOUPDATE;
				}
				sqlite3_free(stmt);

				sqlite3_bind_blob(sqlStmt, 1, peerZID, 12, SQLITE_TRANSIENT);
				sqlite3_bind_text(sqlStmt, 2, selfURI,-1,SQLITE_TRANSIENT);
				sqlite3_bind_text(sqlStmt, 3, peerURI,-1,SQLITE_TRANSIENT);

				ret = sqlite3_step(sqlStmt);
				if (ret!=SQLITE_DONE) {
					return BZRTP_ZIDCACHE_UNABLETOUPDATE;
				}
				sqlite3_finalize(sqlStmt);
				/* get the zuid created */
				*zuid = (int)sqlite3_last_insert_rowid(db);
				return 0;
			}
		} else { /* we had an error querying the DB... */
			return BZRTP_ZIDCACHE_UNABLETOREAD;
		}
	}

	/* retrieve value in column 0 */
	*zuid = sqlite3_column_int(sqlStmt, 0);
	sqlite3_finalize(sqlStmt);

	return 0;
}

/**
 * @brief Write(insert or update) data in cache, adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be inserted, lengths of theses values
 *		All three arrays must be the same lenght: columnsCount
 * 		If the row isn't present in the given table, it will be inserted
 *
 * @param[in/out]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]		zuid		The DB internal id to adress the correct row(binding between local uri and peer ZID+URI)
 * @param[in]		tableName	The name of the table to write in the db, must already exists. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to update
 * @param[in]		values		An array of buffers containing the values to insert/update matching the order of columns array
 * @param[in]		lengths		An array of integer containing the lengths of values array buffer matching the order of columns array
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 *
 * @return 0 on succes, error code otherwise
 */
int bzrtp_cache_write(void *dbPointer, int zuid, char *tableName, char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	char *stmt=NULL;
	int ret,i,j;
	sqlite3_stmt *sqlStmt = NULL;
	sqlite3 *db = (sqlite3 *)dbPointer;
	char *insertColumnsString=NULL;
	int insertColumnsStringLength=0;

	if (dbPointer == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	/* As the zuid row may not be already present in the table, we must run an insert or update SQL command which is not provided by sqlite3 */
	/* UPSERT strategy: run update first and check for changes */

	/* do not perform any check on table name or columns name, the sql statement will just fail when they are wrong */
	/* prepare the columns list string, use the %w formatting option as it shall protect us from anything in the column name  */
	for (i=0; i<columnsCount; i++) {
		insertColumnsStringLength += strlen(columns[i])+5; /* +5 for =?, */
	}
	insertColumnsString = (char *)malloc(++insertColumnsStringLength); /*+1 to add NULL termination */
	sqlite3_snprintf(insertColumnsStringLength,insertColumnsString,"%w=?",columns[0]);
	j=strlen(insertColumnsString);
	for (i=1; i<columnsCount; i++) {
		sqlite3_snprintf(insertColumnsStringLength-j,insertColumnsString+j,", %w=?",columns[i]);
		j=strlen(insertColumnsString);
	}

	stmt = sqlite3_mprintf("UPDATE %w SET %s WHERE zuid=%d;", tableName, insertColumnsString, zuid);
	free(insertColumnsString);
	ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}

	/* bind given values */
	for (i=0; i<columnsCount; i++) {
		sqlite3_bind_blob(sqlStmt, i+1, values[i], lengths[i], SQLITE_TRANSIENT);/* i+1 because index of sql bind is 1 based */
	}

	ret = sqlite3_step(sqlStmt);
	sqlite3_finalize(sqlStmt);

	if (ret!=SQLITE_DONE) {
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}

	/* now check we managed to update a row */
	if (sqlite3_changes(db)==0) { /* update failed: we must insert the row */
		char valuesBindingString[2*columnsCount+3];
		insertColumnsStringLength+=6; /* +6 to add the initial 'zuid, ' to column list */
		insertColumnsString = (char *)malloc(insertColumnsStringLength+6);
		sqlite3_snprintf(insertColumnsStringLength,insertColumnsString,"%w","zuid");
		sprintf(valuesBindingString,"?");

		j=strlen(insertColumnsString);
		for (i=0; i<columnsCount; i++) {
			sqlite3_snprintf(insertColumnsStringLength-j,insertColumnsString+j,", %w",columns[i]);
			j=strlen(insertColumnsString);
			sprintf(valuesBindingString+2*i+1,",?"); /*2 char (,?) for each column plus the initial ? for the zuid column */
		}
		stmt = sqlite3_mprintf("INSERT INTO %w (%s) VALUES(%s);", tableName, insertColumnsString, valuesBindingString);
		free(insertColumnsString);
		ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
		if (ret != SQLITE_OK) {
			return BZRTP_ZIDCACHE_UNABLETOUPDATE;
		}

		/* bind given values */
		sqlite3_bind_int(sqlStmt, 1, zuid);
		for (i=0; i<columnsCount; i++) { /* index of sql bind is 1 based */
			sqlite3_bind_blob(sqlStmt, i+2, values[i], lengths[i], SQLITE_TRANSIENT); /* i+2 because of zuid which is first and 1 based index in binding */
		}

		ret = sqlite3_step(sqlStmt);
		sqlite3_finalize(sqlStmt);

		/* there is a foreign key binding on zuid, which make it impossible to insert a row in zrtp table without an existing zuid */
		/* if it fails it is at this point: TODO: add a specific error return value for this case */
		if (ret!=SQLITE_DONE) {
			return BZRTP_ZIDCACHE_UNABLETOUPDATE;
		}
	}

	return 0;
}

/**
 * @brief Read data from specified table/columns from cache adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be read, and the number of colums to be read
 *		Produce an array of values(uint8_t arrays) and a array of corresponding lengths
 *		Values memory is allocated by this function and must be freed by caller
 *
 * @param[in/out]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]		zuid		The DB internal id to adress the correct row(binding between local uri and peer ZID+URI)
 * @param[in]		tableName	The name of the table to read in the db. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to read, the array's length  is columnsCount
 * @param[out]		values		An array of uint8_t pointers, each one will be allocated to the read value and they must be freed by caller
 * @param[out]		lengths		An array of integer containing the lengths of values array buffer read
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 *
 * @return 0 on succes, error code otherwise
 */
int bzrtp_cache_read(void *dbPointer, int zuid, char *tableName, char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	char *stmt=NULL;
	int ret,i,j;
	sqlite3_stmt *sqlStmt = NULL;
	sqlite3 *db = (sqlite3 *)dbPointer;
	char *readColumnsString=NULL;
	int readColumnsStringLength=0;

	if (dbPointer == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	/* do not perform any check on table name or columns name, the sql statement will just fail when they are wrong */
	/* prepare the columns list string, use the %w formatting option as it shall protect us from anything in the column name  */
	for (i=0; i<columnsCount; i++) {
		readColumnsStringLength += strlen(columns[i])+5; /* +2 for ', '*/
	}
	readColumnsString = (char *)malloc(++readColumnsStringLength); /*+1 to add NULL termination */
	sqlite3_snprintf(readColumnsStringLength,readColumnsString,"%w",columns[0]);
	j=strlen(readColumnsString);
	for (i=1; i<columnsCount; i++) {
		sqlite3_snprintf(readColumnsStringLength-j,readColumnsString+j,", %w",columns[i]);
		j=strlen(readColumnsString);
	}

	stmt = sqlite3_mprintf("SELECT %s FROM %w WHERE zuid=%d LIMIT 1;", readColumnsString, tableName, zuid);
	free(readColumnsString);
	ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	ret = sqlite3_step(sqlStmt);

	if (ret!=SQLITE_ROW) { /* Data was not found or request is not well formed, anyway we don't have the data so just return an error */
		sqlite3_finalize(sqlStmt);
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	/* retrieve values  */
	for (i=0; i<columnsCount; i++) {
		int length;
		length = sqlite3_column_bytes(sqlStmt, i);

		if (length>0) { /* we have rs1 */
			lengths[i] = length;
			values[i] = (uint8_t *)malloc(length*sizeof(uint8_t));
			memcpy(values[i], sqlite3_column_blob(sqlStmt, i), length);
		} else { /* data is null in DB */
			values[i] = NULL;
			lengths[i] = 0;
		}

	}
	sqlite3_finalize(sqlStmt);

	return 0;
}

#else /* ZIDCACHE_ENABLED */

int bzrtp_getSelfZID(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext) {
	/* we are running cacheless, return a random number */
	if (RNGContext != NULL) {
		bctbx_rng_get(RNGContext, selfZID, 12);
	} else {
		return BZRTP_CACHE_DATA_NOTFOUND;
	}
	return 0; 
}

int bzrtp_getPeerAssociatedSecrets(bzrtpContext_t *context, uint8_t peerZID[12]) {
		if (context == NULL) {
		return BZRTP_ZIDCACHE_INVALID_CONTEXT;
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

int bzrtp_cache_write(void *dbPointer, int zuid, char *tableName, char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	return BZRTP_ERROR_CACHEDISABLED;
}

int bzrtp_cache_read(void *dbPointer, int zuid, char *tableName, char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	return BZRTP_ERROR_CACHEDISABLED;
}
#endif /* ZIDCACHE_ENABLED */
