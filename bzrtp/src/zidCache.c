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
#include <stdlib.h>
#include <string.h>
#include "typedef.h"
#include <bctoolbox/crypto.h>
#include "cryptoUtils.h"
#include "zidCache.h"

#ifdef ZIDCACHE_ENABLED
#include "sqlite3.h"
#ifdef HAVE_LIBXML2
#include <libxml/tree.h>
#include <libxml/parser.h>
#endif /* HAVE_LIBXML2 */

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

/* define a version number for the DB schema as an interger MMmmpp */
/* current version is 0.0.2 */
/* Changelog:
 * version 0.0.2 : Add a the active flag in the ziduri table
 * version 0.0.1 : Initial version
 */
#define ZIDCACHE_DBSCHEMA_VERSION_NUMBER 0x000002

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

/**
 * @brief Update the database schema from version 0.0.1 to version 0.0.2
 *
 * Add an integer field 'active' defaulting to 0 in the ziduri table
 *
 * @param[in/out]	db	The sqlite pointer to the table to be updated
 *
 * @return 0 on success, BZRTP_ZIDCACHE_UNABLETOUPDATE otherwise
 */
static int bzrtp_cache_update_000001_to_000002(sqlite3 *db) {
/* create the ziduri table */
	int ret;
	char* errmsg=NULL;
	ret=sqlite3_exec(db,"ALTER TABLE ziduri ADD COLUMN active INTEGER DEFAULT 0;", 0, 0, &errmsg);
	if(ret != SQLITE_OK) {
		sqlite3_free(errmsg);
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}

	return 0;
}

/* ZID cache is split in several tables
 * ziduri : zuid(unique key) | ZID | selfuri | peeruri | active
 *         zuid(ZID/URI binding id) will be used for fastest access to the cache, it binds a local user(self uri/self ZID) to a peer identified both by URI and ZID
 *         active is a flag set to spot the last active peer device associated to an URI. Each time a ZRTP exchange takes place, the active flag is set to one and all other rows with the same peeruri are set to zero
 *         self ZID is stored in this table too in a record having 'self' as peer uri, each local user(uri) has a different ZID 
 *
 * All values except zuid in the following tables are blob, actual integers are split and stored in big endian by callers
 * zrtp : zuid(as foreign key) | rs1 | rs2 | aux secret | pbx secret | pvs flag
 * lime : zuid(as foreign key) | sndKey | rcvKey | sndSId | rcvSId | snd Index | rcv Index | valid
 */
static int bzrtp_initCache_impl(void *dbPointer) {
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
	sqlite3_free(sql);
	if (ret!=SQLITE_OK) {
		sqlite3_free(errmsg);
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	/* Here check version number and provide upgrade is needed */
	if (userVersion != ZIDCACHE_DBSCHEMA_VERSION_NUMBER) {
		if (userVersion > ZIDCACHE_DBSCHEMA_VERSION_NUMBER) { /* nothing to do if we encounter a superior version number than expected, just hope it is compatible */
			//TODO: Log this event
		} else { /* Perform update if needed */
			switch ( userVersion ) {
				case 0x000000 :
					/* nothing to do this is base creation */
					break;
				case 0x000001 :
					ret = bzrtp_cache_update_000001_to_000002(db);
					if (ret != 0) {
						return ret;
					}
					break;
				default : /* nothing particular to do but it shall not append and we shall warn the dev: db schema version was upgraded but no migration function is executed */
					break;
			}
			/* update the schema version in DB metadata */

			sql = sqlite3_mprintf("PRAGMA user_version = %d;",ZIDCACHE_DBSCHEMA_VERSION_NUMBER);
			ret = sqlite3_prepare(db, sql, -1, &stmt, NULL);
			sqlite3_free(sql);
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

	/* if we have an update, we can exit now */
	if (retval == BZRTP_CACHE_UPDATE) {
		return BZRTP_CACHE_UPDATE;
	}

	/* create the ziduri table */
	ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS ziduri ("
							"zuid		INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,"
							"zid		BLOB NOT NULL DEFAULT '000000000000',"
							"selfuri	TEXT NOT NULL DEFAULT 'unset',"
							"peeruri	TEXT NOT NULL DEFAULT 'unset',"
							"active		INTEGER DEFAULT 0"
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

/* non locking database version of the previous function, is deprecated but kept for compatibility */
int bzrtp_initCache(void *dbPointer) {
	return bzrtp_initCache_impl(dbPointer);
}

/* locking database version of the previous function */
int bzrtp_initCache_lock(void *dbPointer, bctbx_mutex_t *zidCacheMutex) {
	int retval;

	if (dbPointer != NULL && zidCacheMutex != NULL) {
		bctbx_mutex_lock(zidCacheMutex);
		sqlite3_exec((sqlite3 *)dbPointer, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		retval = bzrtp_initCache_impl(dbPointer);
		if (retval == 0 || retval == BZRTP_CACHE_UPDATE || retval == BZRTP_CACHE_SETUP) {
			sqlite3_exec((sqlite3 *)dbPointer, "COMMIT;", NULL, NULL, NULL);
		} else {
			sqlite3_exec((sqlite3 *)dbPointer, "ROLLBACK;", NULL, NULL, NULL);
		}
		bctbx_mutex_unlock(zidCacheMutex);
		return retval;
	}
	else {
		return bzrtp_initCache_impl(dbPointer);
	}
}

static int bzrtp_getSelfZID_impl(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext) {
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
/* non locking database version of the previous function, is deprecated but kept for compatibility */
int bzrtp_getSelfZID(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext) {
	return bzrtp_getSelfZID_impl(dbPointer, selfURI, selfZID, RNGContext);
}
/* locking database version of the previous function */
int bzrtp_getSelfZID_lock(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext, bctbx_mutex_t *zidCacheMutex) {
	int retval;

	if (dbPointer != NULL && zidCacheMutex != NULL) {
		bctbx_mutex_lock(zidCacheMutex);
		sqlite3_exec((sqlite3 *)dbPointer, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		retval = bzrtp_getSelfZID_impl(dbPointer, selfURI, selfZID, RNGContext);
		if (retval == 0) {
			sqlite3_exec((sqlite3 *)dbPointer, "COMMIT;", NULL, NULL, NULL);
		} else {
			sqlite3_exec((sqlite3 *)dbPointer, "ROLLBACK;", NULL, NULL, NULL);
		}
		bctbx_mutex_unlock(zidCacheMutex);
		return retval;
	}
	else {
		return bzrtp_getSelfZID_impl(dbPointer, selfURI, selfZID, RNGContext);
	}
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

	if (context->zidCacheMutex != NULL) {
		bctbx_mutex_lock(context->zidCacheMutex);
	}

	/* get all secrets from zrtp table, ORDER BY is just to ensure consistent return in case of inconsistent table) */
	stmt = sqlite3_mprintf("SELECT z.zuid, z.rs1, z.rs2, z.aux, z.pbx, z.pvs FROM ziduri as zu INNER JOIN zrtp as z ON z.zuid=zu.zuid WHERE zu.selfuri=? AND zu.peeruri=? AND zu.zid=? ORDER BY zu.zuid LIMIT 1;");
	ret = sqlite3_prepare_v2(context->zidCache, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		if (context->zidCacheMutex != NULL) {
			bctbx_mutex_unlock(context->zidCacheMutex);
		}
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}
	sqlite3_bind_text(sqlStmt, 1, context->selfURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_text(sqlStmt, 2, context->peerURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_blob(sqlStmt, 3, peerZID, 12, SQLITE_TRANSIENT);

	ret = sqlite3_step(sqlStmt);

	if (ret!=SQLITE_ROW) {
		sqlite3_finalize(sqlStmt);
		if (ret == SQLITE_DONE) {/* not found in cache, just leave cached secrets reset, but retrieve zuid, do not insert new peer ZID at this step, it must be done only when negotiation succeeds */
			/* last param is NULL as we already hold the lock on database */
			ret = bzrtp_cache_getZuid((void *)context->zidCache, context->selfURI, context->peerURI, context->peerZID, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &context->zuid, NULL);
		} else { /* we had an error querying the DB... */
			ret = BZRTP_ZIDCACHE_UNABLETOREAD;
		}
		if (context->zidCacheMutex != NULL) {
			bctbx_mutex_unlock(context->zidCacheMutex);
		}
		return ret;
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

	if (context->zidCacheMutex != NULL) {
		bctbx_mutex_unlock(context->zidCacheMutex);
	}
	return 0;
}


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
 * 					if identity binding is not found and insertFlag set to BZRTP_ZIDCACHE_DONT_INSERT_ZUID, this value is set to 0
 * @param[in]		zidCacheMutex	Points to a mutex used to lock zidCache database access, ignored if NULL
 *
 * @return 0 on success, BZRTP_ERROR_CACHE_PEERNOTFOUND if peer was not in and the insert flag is not set to BZRTP_ZIDCACHE_INSERT_ZUID, error code otherwise
 */
int bzrtp_cache_getZuid(void *dbPointer, const char *selfURI, const char *peerURI, const uint8_t peerZID[12], const uint8_t insertFlag, int *zuid, bctbx_mutex_t *zidCacheMutex) {
	char *stmt=NULL;
	int ret;
	sqlite3_stmt *sqlStmt = NULL;
	sqlite3 *db = (sqlite3 *)dbPointer;

	if (dbPointer == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	if (zidCacheMutex != NULL) {
		bctbx_mutex_lock(zidCacheMutex);
	}

	/* Try to fetch the requested zuid */
	stmt = sqlite3_mprintf("SELECT zuid FROM ziduri WHERE selfuri=? AND peeruri=? AND zid=? ORDER BY zuid LIMIT 1;");
	ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		if (zidCacheMutex != NULL) {
			bctbx_mutex_unlock(zidCacheMutex);
		}
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	sqlite3_bind_text(sqlStmt, 1, selfURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_text(sqlStmt, 2, peerURI,-1,SQLITE_TRANSIENT);
	sqlite3_bind_blob(sqlStmt, 3, peerZID, 12, SQLITE_TRANSIENT);

	ret = sqlite3_step(sqlStmt);

	if (ret!=SQLITE_ROW) { /* We didn't found this binding in the DB */
		sqlite3_finalize(sqlStmt);
		if (ret == SQLITE_DONE) { /* query executed correctly, just our data is not there */
			/* shall we insert it? */
			if (insertFlag == BZRTP_ZIDCACHE_INSERT_ZUID) {
				uint8_t *localZID = NULL;
				char *errmsg=NULL;

				/* check that we have a self ZID matching the self URI and insert a new row */
				stmt = sqlite3_mprintf("SELECT zid FROM ziduri WHERE selfuri=%Q AND peeruri='self' ORDER BY zuid LIMIT 1;",selfURI);
				ret = sqlite3_exec(db,stmt,callback_getSelfZID,&localZID,&errmsg);
				sqlite3_free(stmt);
				if (ret != SQLITE_OK) {
					sqlite3_free(errmsg);
					if (zidCacheMutex != NULL) {
						bctbx_mutex_unlock(zidCacheMutex);
					}
					return BZRTP_ZIDCACHE_UNABLETOREAD;
				}

				if (localZID==NULL) { /* this sip URI is not in our DB, do not create an association with the peer ZID/URI binding */
					if (zidCacheMutex != NULL) {
						bctbx_mutex_unlock(zidCacheMutex);
					}
					return BZRTP_ZIDCACHE_BADINPUTDATA;
				} else { /* yes we know this URI on local device, add a row in the ziduri table */
					free(localZID);
					stmt = sqlite3_mprintf("INSERT INTO ziduri (zid,selfuri,peeruri) VALUES(?,?,?);");
					ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
					if (ret != SQLITE_OK) {
						if (zidCacheMutex != NULL) {
							bctbx_mutex_unlock(zidCacheMutex);
						}
						return BZRTP_ZIDCACHE_UNABLETOUPDATE;
					}
					sqlite3_free(stmt);

					sqlite3_bind_blob(sqlStmt, 1, peerZID, 12, SQLITE_TRANSIENT);
					sqlite3_bind_text(sqlStmt, 2, selfURI,-1,SQLITE_TRANSIENT);
					sqlite3_bind_text(sqlStmt, 3, peerURI,-1,SQLITE_TRANSIENT);

					ret = sqlite3_step(sqlStmt);
					if (ret!=SQLITE_DONE) {
						if (zidCacheMutex != NULL) {
							bctbx_mutex_unlock(zidCacheMutex);
						}
						return BZRTP_ZIDCACHE_UNABLETOUPDATE;
					}
					sqlite3_finalize(sqlStmt);
					/* get the zuid created */
					*zuid = (int)sqlite3_last_insert_rowid(db);
					if (zidCacheMutex != NULL) {
						bctbx_mutex_unlock(zidCacheMutex);
					}
					return 0;
				}
			} else { /* no found and not inserted */
				*zuid = 0;
				if (zidCacheMutex != NULL) {
					bctbx_mutex_unlock(zidCacheMutex);
				}
				return BZRTP_ERROR_CACHE_PEERNOTFOUND;
			}
		} else { /* we had an error querying the DB... */
			if (zidCacheMutex != NULL) {
				bctbx_mutex_unlock(zidCacheMutex);
			}
			return BZRTP_ZIDCACHE_UNABLETOREAD;
		}
	}

	/* retrieve value in column 0 */
	*zuid = sqlite3_column_int(sqlStmt, 0);
	sqlite3_finalize(sqlStmt);

	if (zidCacheMutex != NULL) {
		bctbx_mutex_unlock(zidCacheMutex);
	}

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
static int bzrtp_cache_write_impl(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	char *stmt=NULL;
	int ret,i,j;
	sqlite3_stmt *sqlStmt = NULL;
	sqlite3 *db = (sqlite3 *)dbPointer;
	char *insertColumnsString=NULL;
	int insertColumnsStringLength=0;

	if (dbPointer == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	if (zuid == 0) { /* we're giving an invalid zuid, means we were not able to retrieve it previously, just do nothing */
		return BZRTP_ERROR_CACHE_PEERNOTFOUND;
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
		char *valuesBindingString = alloca(2*columnsCount+3);
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
		sqlite3_free(stmt);
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

/* non locking database version of the previous function, is deprecated but kept for compatibility */
int bzrtp_cache_write(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	return bzrtp_cache_write_impl(dbPointer, zuid, tableName, columns, values, lengths, columnsCount);
}

/* locking database version of the previous function */
int bzrtp_cache_write_lock(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount, bctbx_mutex_t *zidCacheMutex) {
	int retval;


	if (dbPointer != NULL && zidCacheMutex != NULL) {
		bctbx_mutex_lock(zidCacheMutex);
		sqlite3_exec((sqlite3 *)dbPointer, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		retval = bzrtp_cache_write_impl(dbPointer, zuid, tableName, columns, values, lengths, columnsCount);
		if (retval == 0) {
			sqlite3_exec((sqlite3 *)dbPointer, "COMMIT;", NULL, NULL, NULL);
		} else {
			sqlite3_exec((sqlite3 *)dbPointer, "ROLLBACK;", NULL, NULL, NULL);
		}
		bctbx_mutex_unlock(zidCacheMutex);
		return retval;
	}
	else {
		return bzrtp_cache_write_impl(dbPointer, zuid, tableName, columns, values, lengths, columnsCount);
	}
}


/**
 * @brief This is a convenience wrapper to the bzrtp_cache_write function which will also take care of
 *        setting the ziduri table 'active' flag to one for the current row and reset all other rows with matching peeruri
 *
 * Write(insert or update) data in cache, adressing it by zuid (ZID/URI binding id used in cache)
 * 		Get arrays of column names, values to be inserted, lengths of theses values
 *		All three arrays must be the same lenght: columnsCount
 * 		If the row isn't present in the given table, it will be inserted
 *
 * @param[in/out]	context		the current context, used to get the cache db pointer, zuid and cache mutex
 * @param[in]		tableName	The name of the table to write in the db, must already exists. Null terminated string
 * @param[in]		columns		An array of null terminated strings containing the name of the columns to update
 * @param[in]		values		An array of buffers containing the values to insert/update matching the order of columns array
 * @param[in]		lengths		An array of integer containing the lengths of values array buffer matching the order of columns array
 * @param[in]		columnsCount	length common to columns,values and lengths arrays
 *
 * @return 0 on succes, error code otherwise
 */
int bzrtp_cache_write_active(bzrtpContext_t *context, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	char *stmt=NULL;
	int ret;
	const unsigned char *peeruri=NULL;
	int activeFlag=0;

	sqlite3_stmt *sqlStmt = NULL;

	/* initial checks */
	if (context->zidCache == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	if (context->zuid == 0) { /* we're giving an invalid zuid, means we were not able to retrieve it previously, just do nothing */
		return BZRTP_ERROR_CACHE_PEERNOTFOUND;
	}

	if (context->zidCacheMutex != NULL) {
		bctbx_mutex_lock(context->zidCacheMutex);
	}
	sqlite3_exec(context->zidCache, "BEGIN TRANSACTION;", NULL, NULL, NULL);

	/* Retrieve the peerUri and active flag from ziduri table */
	stmt = sqlite3_mprintf("SELECT peeruri, active FROM ziduri WHERE zuid=? LIMIT 1;");
	ret = sqlite3_prepare_v2(context->zidCache, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		sqlite3_exec(context->zidCache, "ROLLBACK;", NULL, NULL, NULL);
		if (context->zidCacheMutex != NULL) {
			bctbx_mutex_unlock(context->zidCacheMutex);
		}
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	sqlite3_bind_int(sqlStmt, 1, context->zuid);

	ret = sqlite3_step(sqlStmt);

	if (ret!=SQLITE_ROW) { /* We didn't found this zuid in the DB -> we would not be able to write */
		sqlite3_finalize(sqlStmt);
		sqlite3_exec(context->zidCache, "ROLLBACK;", NULL, NULL, NULL);
		if (context->zidCacheMutex != NULL) {
			bctbx_mutex_unlock(context->zidCacheMutex);
		}
		return BZRTP_ZIDCACHE_UNABLETOUPDATE;
	}

	/* retrieve values 0:peeruri, 1:active */
	peeruri = sqlite3_column_text(sqlStmt, 0); /* warning: finalize the statement will invalidate peeruri pointer */
	activeFlag = sqlite3_column_int(sqlStmt, 1);

	/* if active flag is already set, just do nothing otherwise set it and reset all others with the same peeruri(active device is shared among local users) */
	if (activeFlag == 0) {
		sqlite3_stmt *sqlStmtActive = NULL;
		/* reset all active flags with this peeruri */
		stmt = sqlite3_mprintf("UPDATE ziduri SET active=0 WHERE active<>0 AND zuid<>? AND peeruri=?;");
		ret = sqlite3_prepare_v2(context->zidCache, stmt, -1, &sqlStmtActive, NULL);
		sqlite3_free(stmt);
		if (ret != SQLITE_OK) {
			sqlite3_finalize(sqlStmt);
			sqlite3_finalize(sqlStmtActive);
			sqlite3_exec(context->zidCache, "ROLLBACK;", NULL, NULL, NULL);
			if (context->zidCacheMutex != NULL) {
				bctbx_mutex_unlock(context->zidCacheMutex);
			}
			return BZRTP_ZIDCACHE_UNABLETOREAD;
		}
		sqlite3_bind_int(sqlStmtActive, 1, context->zuid);
		sqlite3_bind_text(sqlStmtActive, 2, (const char *)peeruri, -1, SQLITE_TRANSIENT);
		ret = sqlite3_step(sqlStmtActive);
		sqlite3_finalize(sqlStmtActive);
		/* set to 1 the active flag four current row */
		stmt = sqlite3_mprintf("UPDATE ziduri SET active=1 WHERE zuid=?;");
		ret = sqlite3_prepare_v2(context->zidCache, stmt, -1, &sqlStmtActive, NULL);
		sqlite3_free(stmt);
		if (ret != SQLITE_OK) {
			sqlite3_finalize(sqlStmt);
			sqlite3_finalize(sqlStmtActive);
			sqlite3_exec(context->zidCache, "ROLLBACK;", NULL, NULL, NULL);
			if (context->zidCacheMutex != NULL) {
				bctbx_mutex_unlock(context->zidCacheMutex);
			}
			return BZRTP_ZIDCACHE_UNABLETOREAD;
		}
		sqlite3_bind_int(sqlStmtActive, 1, context->zuid);
		ret = sqlite3_step(sqlStmtActive);
		sqlite3_finalize(sqlStmtActive);
	}

	sqlite3_finalize(sqlStmt);

	/* and perform the actual writing */
	ret = bzrtp_cache_write_impl(context->zidCache, context->zuid, tableName, columns, values, lengths, columnsCount);

	if (ret == 0) {
		sqlite3_exec(context->zidCache, "COMMIT;", NULL, NULL, NULL);
	} else {
		sqlite3_exec(context->zidCache, "ROLLBACK;", NULL, NULL, NULL);
	}

	if (context->zidCacheMutex != NULL) {
		bctbx_mutex_unlock(context->zidCacheMutex);
	}

	return ret;
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
static int bzrtp_cache_read_impl(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
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
/* non locking database version of the previous function, is deprecated but kept for compatibility */
int bzrtp_cache_read(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	return bzrtp_cache_read_impl(dbPointer, zuid, tableName, columns, values, lengths, columnsCount);
}
/* locking database version of the previous function */
int bzrtp_cache_read_lock(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount, bctbx_mutex_t *zidCacheMutex) {
	int retval;

	if (dbPointer != NULL && zidCacheMutex != NULL) {
		bctbx_mutex_lock(zidCacheMutex);
		retval = bzrtp_cache_read_impl(dbPointer, zuid, tableName, columns, values, lengths, columnsCount);
		bctbx_mutex_unlock(zidCacheMutex);
		return retval;
	}
	else {
		return bzrtp_cache_read_impl(dbPointer, zuid, tableName, columns, values, lengths, columnsCount);
	}
}

/**
 * @brief Perform migration from xml version to sqlite3 version of cache
 *	Warning: new version of cache associate a ZID to each local URI, the old one did not
 *		the migration function will associate any data in the cache to the sip URI given in parameter which shall be the default URI
 * @param[in]		cacheXml	a pointer to an xmlDocPtr structure containing the old cache to be migrated
 * @param[in/out]	cacheSqlite	a pointer to an sqlite3 structure containing a cache initialised using bzrtp_cache_init function
 * @param[in]		selfURI		default sip URI for this end point, NULL terminated char
 *
 * @return	0 on success, BZRTP_ERROR_CACHEDISABLED when bzrtp was not compiled with cache enabled, BZRTP_ERROR_CACHEMIGRATIONFAILED on error during migration
 */
int bzrtp_cache_migration(void *cacheXmlPtr, void *cacheSqlite, const char *selfURI) {
#ifdef HAVE_LIBXML2
	if (cacheXmlPtr) {
		xmlDocPtr cacheXml = (xmlDocPtr)cacheXmlPtr;
		xmlNodePtr cur;
		xmlChar *selfZidHex=NULL;
		uint8_t selfZID[12];
		sqlite3 *db = (sqlite3 *)cacheSqlite;
		sqlite3_stmt *sqlStmt = NULL;
		int ret;

		/* parse the cache to get the selfZID and insert it in sqlcache */
		cur = xmlDocGetRootElement(cacheXml);
		/* if we found a root element, parse its children node */
		if (cur!=NULL)
		{
			cur = cur->xmlChildrenNode;
		}
		selfZidHex = NULL;
		while (cur!=NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *)"selfZID"))){ /* self ZID found, extract it */
				selfZidHex = xmlNodeListGetString(cacheXml, cur->xmlChildrenNode, 1);
				bctbx_str_to_uint8(selfZID, selfZidHex, 24);
				break;
			}
			cur = cur->next;
		}
		/* did we found a self ZID? */
		if (selfZidHex == NULL) {
			bctbx_warning("ZRTP/LIME cache migration: Failed to parse selfZID");
			return BZRTP_ERROR_CACHEMIGRATIONFAILED;
		}

		/* insert the selfZID in cache, associate it to default local sip:uri in case we have more than one */
		bctbx_message("ZRTP/LIME cache migration: found selfZID %.24s link it to default URI %s in SQL cache", selfZidHex, selfURI);
		xmlFree(selfZidHex);

		ret = sqlite3_prepare_v2(db, "INSERT INTO ziduri (zid,selfuri,peeruri) VALUES(?,?,?);", -1, &sqlStmt, NULL);
		if (ret != SQLITE_OK) {
			bctbx_warning("ZRTP/LIME cache migration: Failed to insert selfZID");
			return BZRTP_ERROR_CACHEMIGRATIONFAILED;
		}
		sqlite3_bind_blob(sqlStmt, 1, selfZID, 12, SQLITE_TRANSIENT);
		sqlite3_bind_text(sqlStmt, 2, selfURI,-1,SQLITE_TRANSIENT);
		sqlite3_bind_text(sqlStmt, 3, "self",-1,SQLITE_TRANSIENT);

		ret = sqlite3_step(sqlStmt);
		if (ret!=SQLITE_DONE) {
			bctbx_warning("ZRTP/LIME cache migration: Failed to insert selfZID");
			return BZRTP_ERROR_CACHEMIGRATIONFAILED;
		}
		sqlite3_finalize(sqlStmt);

		/* loop over all the peer node in the xml cache and get from them : uri(can be more than one), ZID, rs1, rs2, pvs, sndKey, rcvKey, sndSId, rcvSId, sndIndex, rcvIndex, valid */
		/* some of these may be missing(pvs, valid, rs2) but we'll consider them NULL */
		/* aux and pbx secrets were not used, so don't even bother looking for them */
		cur = xmlDocGetRootElement(cacheXml)->xmlChildrenNode;

		while (cur!=NULL) { /* loop on all peer nodes */
			if ((!xmlStrcmp(cur->name, (const xmlChar *)"peer"))) { /* found a peer node, check if there is a sipURI node in it (other nodes are just ignored) */
				int i;
				xmlNodePtr peerNodeChildren = cur->xmlChildrenNode;
				xmlChar *nodeContent = NULL;
				xmlChar *peerZIDString = NULL;
				uint8_t peerZID[12];
				uint8_t peerZIDFound=0;
				xmlChar *peerUri[128]; /* array to contain all the peer uris found in one node */
				/* hopefully they won't be more than 128(it would mean some peer has more than 128 accounts and we called all of them...) */
				int peerUriIndex=0; /* index of previous array */
				const char *zrtpColNames[] = {"rs1", "rs2", "pvs"};
				uint8_t *zrtpColValues[] = {NULL, NULL, NULL};
				size_t zrtpColExpectedLengths[] = {32,32,1};
				size_t zrtpColLengths[] = {0,0,0};

				const char *limeColNames[] = {"sndKey", "rcvKey", "sndSId", "rcvSId", "sndIndex", "rcvIndex", "valid"};
				uint8_t *limeColValues[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
				size_t limeColExpectedLengths[] = {32,32,32,32,4,4,8};
				size_t limeColLengths[] = {0,0,0,0,0,0,0};

				/* check all the children nodes to retrieve all information we may get */
				while (peerNodeChildren!=NULL && peerUriIndex<128) {
					if (!xmlStrcmp(peerNodeChildren->name, (const xmlChar *)"uri")) { /* found a peer an URI node, get the content */
						peerUri[peerUriIndex] = xmlNodeListGetString(cacheXml, peerNodeChildren->xmlChildrenNode, 1);
						peerUriIndex++;
					}

					if (!xmlStrcmp(peerNodeChildren->name, (const xmlChar *)"ZID")) {
						peerZIDString = xmlNodeListGetString(cacheXml, peerNodeChildren->xmlChildrenNode, 1);
						bctbx_str_to_uint8(peerZID, peerZIDString, 24);

						peerZIDFound=1;
					}

					for (i=0; i<3; i++) {
						if (!xmlStrcmp(peerNodeChildren->name, (const xmlChar *)zrtpColNames[i])) {
							nodeContent = xmlNodeListGetString(cacheXml, peerNodeChildren->xmlChildrenNode, 1);
							zrtpColValues[i] = (uint8_t *)bctbx_malloc(zrtpColExpectedLengths[i]);
							bctbx_str_to_uint8(zrtpColValues[i], nodeContent, 2*zrtpColExpectedLengths[i]);
							zrtpColLengths[i]=zrtpColExpectedLengths[i];
						}
					}

					for (i=0; i<7; i++) {
						if (!xmlStrcmp(peerNodeChildren->name, (const xmlChar *)limeColNames[i])) {
							nodeContent = xmlNodeListGetString(cacheXml, peerNodeChildren->xmlChildrenNode, 1);
							limeColValues[i] = (uint8_t *)bctbx_malloc(limeColExpectedLengths[i]);
							bctbx_str_to_uint8(limeColValues[i], nodeContent, 2*limeColExpectedLengths[i]);
							limeColLengths[i]=limeColExpectedLengths[i];
						}
					}

					peerNodeChildren = peerNodeChildren->next;
					xmlFree(nodeContent);
					nodeContent=NULL;
				}

				if (peerUriIndex>0 && peerZIDFound==1) { /* we found at least an uri in this peer node, extract the keys all other informations */
					/* retrieve all the informations */

					/* loop over all the uri founds */
					for (i=0; i<peerUriIndex; i++) {
						char *stmt = NULL;
						int zuid;
						/* create the entry in the ziduri table (it will give us the zuid to be used to insert infos in lime and zrtp tables) */
						/* we could use directly the bzrtp_cache_getZuid function, but avoid useless query by directly inserting the data */
						stmt = sqlite3_mprintf("INSERT INTO ziduri (zid,selfuri,peeruri) VALUES(?,?,?);");
						ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
						if (ret != SQLITE_OK) {
							bctbx_warning("ZRTP/LIME cache migration: Failed to insert peer ZID %s", peerUri[i]);
							return BZRTP_ERROR_CACHEMIGRATIONFAILED;
						}
						sqlite3_free(stmt);

						sqlite3_bind_blob(sqlStmt, 1, peerZID, 12, SQLITE_TRANSIENT);
						sqlite3_bind_text(sqlStmt, 2, selfURI, -1, SQLITE_TRANSIENT);
						sqlite3_bind_text(sqlStmt, 3, (const char *)(peerUri[i]), -1, SQLITE_TRANSIENT);

						ret = sqlite3_step(sqlStmt);
						if (ret!=SQLITE_DONE) {
							bctbx_warning("ZRTP/LIME cache migration: Failed to insert peer ZID %s", peerUri[i]);
							return BZRTP_ERROR_CACHEMIGRATIONFAILED;
						}
						sqlite3_finalize(sqlStmt);
						/* get the zuid created */
						zuid = (int)sqlite3_last_insert_rowid(db);

						bctbx_message("ZRTP/LIME cache migration: Inserted self %s peer %s ZID %s sucessfully with zuid %d\n", selfURI, peerUri[i], peerZIDString, zuid);
						xmlFree(peerUri[i]);
						peerUri[i]=NULL;

						/* now insert data in the zrtp and lime table, keep going even if it fails */
						if ((ret=bzrtp_cache_write_impl(db, zuid, "zrtp", zrtpColNames, zrtpColValues, zrtpColLengths, 3)) != 0) {
							bctbx_error("ZRTP/LIME cache migration: could not insert data in zrtp table, return value %x", ret);
						}
						if ((ret=bzrtp_cache_write_impl(db, zuid, "lime", limeColNames, limeColValues, limeColLengths, 7)) != 0) {
							bctbx_error("ZRTP/LIME cache migration: could not insert data in lime table, return value %x", ret);
						}
					}
				}
				bctbx_free(zrtpColValues[0]);
				bctbx_free(zrtpColValues[1]);
				bctbx_free(zrtpColValues[2]);
				for (i=0; i<7; i++) {
					bctbx_free(limeColValues[i]);
				}
				xmlFree(peerZIDString);
			}
			cur = cur->next;
		}
		return 0;
	}
	return BZRTP_ERROR_CACHEMIGRATIONFAILED;
#else /* HAVE_LIBXML2 */
	bctbx_error("ZRTP/LIME cache migration: could not perform migration as LIBMXL2 is not linked to bzrtp.");
	return BZRTP_ERROR_CACHEMIGRATIONFAILED;
#endif /* HAVE_LIBXML2 */
}

/*
 * @brief Retrieve from bzrtp cache the trust status(based on the previously verified flag) of a peer URI
 *
 * This function will return the SAS validation status of the active device
 * associated to the given peerURI.
 *
 * Important note about the active device:
 * - any ZRTP exchange with a peer device will set it to be the active one for its sip:uri
 * - the concept of active device is shared between local accounts if there are several of them, it means that :
 *       - if you have several local users on your device, each of them may have an entry in the ZRTP cache with a particular peer sip:uri (if they ever got in contact with it) but only one of this entry is set to active
 *       - this function will return the status associated to the last updated entry without any consideration for the local users it is associated with
 * - any call where the SAS was neither accepted or rejected will not update the trust status but will set as active device for the peer sip:uri the one involved in the call
 *
 * This function is intended for use in a mono-device environment.
 *
 * @param[in]	dbPointer	Pointer to an already opened sqlite db
 * @param[in]	peerURI		The peer sip:uri we're interested in
 *
 * @return one of:
 *  - BZRTP_CACHE_PEER_STATUS_UNKNOWN : this uri is not present in cache OR during calls with the active device, SAS never was validated or rejected
 *  	Note: once the SAS has been validated or rejected, the status will never return to UNKNOWN(unless you delete your cache)
 *  - BZRTP_CACHE_PEER_STATUS_VALID : the active device status is set to valid
 *  - BZRTP_CACHE_PEER_STATUS_INVALID : the active peer device status is set to invalid
 *
 */
int bzrtp_cache_getPeerStatus_lock(void *dbPointer, const char *peerURI, bctbx_mutex_t *zidCacheMutex) {
	char *stmt=NULL;
	int ret,retval = BZRTP_CACHE_PEER_STATUS_UNKNOWN;

	sqlite3_stmt *sqlStmt = NULL;
	sqlite3 *db = (sqlite3 *)dbPointer;

	/* initial checks */
	if (dbPointer == NULL) { /* we are running cacheless */
		return BZRTP_ZIDCACHE_RUNTIME_CACHELESS;
	}

	if (zidCacheMutex != NULL) {
		bctbx_mutex_lock(zidCacheMutex);
	}

	/* Retrieve the pvs flag from zrtp table for the given peerURI
	 * Order by active desc so we get the row with the active flag set to 1
	 * if there is no such row(just after migration from version 0.0.1 of DB schema
	 * we will get the last device inserted for this peer, it is the most likely to be the active one in the context of a mono-device environment) */
	stmt = sqlite3_mprintf("SELECT z.pvs FROM ziduri as zu INNER JOIN zrtp as z ON z.zuid=zu.zuid WHERE zu.peeruri=? ORDER BY zu.active DESC,zu.zuid DESC LIMIT 1;");
	ret = sqlite3_prepare_v2(db, stmt, -1, &sqlStmt, NULL);
	sqlite3_free(stmt);
	if (ret != SQLITE_OK) {
		if (zidCacheMutex != NULL) {
			bctbx_mutex_unlock(zidCacheMutex);
		}
		return BZRTP_ZIDCACHE_UNABLETOREAD;
	}

	sqlite3_bind_text(sqlStmt, 1, peerURI, -1, SQLITE_TRANSIENT);

	ret = sqlite3_step(sqlStmt);

	if (ret==SQLITE_ROW) { /* We found the peerURI in the DB */
		/* pvs is stored as blob in memory, just get the first byte(length shall be one anyway) */
		/* it may be NULL -> return UNKNOWN */
		int length = sqlite3_column_bytes(sqlStmt, 0);
		if (length!=1) { /* value is NULL(or we have something that is not 0x01 or 0x00) in db, we do not know the status of this device */
			retval = BZRTP_CACHE_PEER_STATUS_UNKNOWN;
		} else {
			if (*((uint8_t *)sqlite3_column_blob(sqlStmt, 0)) == 0x01) {
				retval = BZRTP_CACHE_PEER_STATUS_VALID;
			} else {
				retval = BZRTP_CACHE_PEER_STATUS_INVALID;
			}
		}
	} else { /* the peerURI was not found in DB */
		/* Note: if ret != SQLITE_DONE, we had an error when accessing the database,
		 * we just swallow it and return unknown status but signal it in traces */
		if (ret != SQLITE_DONE) {
			bctbx_warning("Querying DB for peer(%s) status returned an sqlite error code %d\n", peerURI, ret);
		}
		retval = BZRTP_CACHE_PEER_STATUS_UNKNOWN;
	}

	sqlite3_finalize(sqlStmt);

	if (zidCacheMutex != NULL) {
		bctbx_mutex_unlock(zidCacheMutex);
	}
	return retval;
}

#else /* ZIDCACHE_ENABLED */

static int bzrtp_getSelfZID_impl(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext) {
	/* we are running cacheless, return a random number */
	if (RNGContext != NULL) {
		bctbx_rng_get(RNGContext, selfZID, 12);
	} else {
		return BZRTP_CACHE_DATA_NOTFOUND;
	}
	return 0; 
}
int bzrtp_getSelfZID(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext) {
	return bzrtp_getSelfZID_impl(dbPointer, selfURI, selfZID, RNGContext);
}
int bzrtp_getSelfZID_lock(void *dbPointer, const char *selfURI, uint8_t selfZID[12], bctbx_rng_context_t *RNGContext, bctbx_mutex_t *zidCacheMutex) {
	return bzrtp_getSelfZID_impl(dbPointer, selfURI, selfZID, RNGContext);
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

int bzrtp_cache_write_active(bzrtpContext_t *context, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	return BZRTP_ERROR_CACHEDISABLED;
}

int bzrtp_cache_write(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	return BZRTP_ERROR_CACHEDISABLED;
}

int bzrtp_cache_write_lock(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount, bctbx_mutex_t *zidCacheMutex) {
	return BZRTP_ERROR_CACHEDISABLED;
}

int bzrtp_cache_read(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount) {
	return BZRTP_ERROR_CACHEDISABLED;
}

int bzrtp_cache_read_lock(void *dbPointer, int zuid, const char *tableName, const char **columns, uint8_t **values, size_t *lengths, uint8_t columnsCount, bctbx_mutex_t *zidCacheMutex) {
	return BZRTP_ERROR_CACHEDISABLED;
}

int bzrtp_cache_migration(void *cacheXml, void *cacheSqlite, const char *selfURI) {
	return BZRTP_ERROR_CACHEDISABLED;
}

int bzrtp_cache_getPeerStatus_lock(void *dbPointer, const char *peerURI, bctbx_mutex_t *zidCacheMutex) {
	return BZRTP_CACHE_PEER_STATUS_UNKNOWN;
}

int bzrtp_cache_getZuid(void *dbPointer, const char *selfURI, const char *peerURI, const uint8_t peerZID[12], const uint8_t insertFlag, int *zuid, bctbx_mutex_t *zidCacheMutex) {
	return BZRTP_ERROR_CACHEDISABLED;
}
#endif /* ZIDCACHE_ENABLED */
