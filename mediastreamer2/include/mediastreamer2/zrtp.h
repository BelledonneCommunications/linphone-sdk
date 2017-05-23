/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2014 Belledonne Communications

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ms_zrtp_h
#define ms_zrtp_h

#include <ortp/rtpsession.h>
#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C"{
#endif

/* defined in mediastream.h */
struct _MSMediaStreamSessions;


/* Error codes */
#define MSZRTP_ERROR_CHANNEL_ALREADY_STARTED		-0x0001

#define MS_MAX_ZRTP_CRYPTO_TYPES 7

/* cache related function return codes */
#define MSZRTP_CACHE_ERROR		-0x1000
#define MSZRTP_CACHE_SETUP		0x2000
#define MSZRTP_CACHE_UPDATE		0x2001
#define MSZRTP_ERROR_CACHEDISABLED				-0x0200
#define MSZRTP_ERROR_CACHEMIGRATIONFAILED			-0x0400



typedef uint8_t MsZrtpCryptoTypesCount;

typedef enum _MSZrtpHash{
	MS_ZRTP_HASH_INVALID,
	MS_ZRTP_HASH_S256,
	MS_ZRTP_HASH_S384,
	MS_ZRTP_HASH_N256,
	MS_ZRTP_HASH_N384
} MSZrtpHash;

typedef enum _MSZrtpCipher{
	MS_ZRTP_CIPHER_INVALID,
	MS_ZRTP_CIPHER_AES1,
	MS_ZRTP_CIPHER_AES2,
	MS_ZRTP_CIPHER_AES3,
	MS_ZRTP_CIPHER_2FS1,
	MS_ZRTP_CIPHER_2FS2,
	MS_ZRTP_CIPHER_2FS3
} MSZrtpCipher;

typedef enum _MSZrtpAuthTag{
	MS_ZRTP_AUTHTAG_INVALID,
	MS_ZRTP_AUTHTAG_HS32,
	MS_ZRTP_AUTHTAG_HS80,
	MS_ZRTP_AUTHTAG_SK32,
	MS_ZRTP_AUTHTAG_SK64
} MSZrtpAuthTag;

typedef enum _MSZrtpKeyAgreement{
	MS_ZRTP_KEY_AGREEMENT_INVALID,
	MS_ZRTP_KEY_AGREEMENT_DH2K,
	MS_ZRTP_KEY_AGREEMENT_DH3K,
	MS_ZRTP_KEY_AGREEMENT_EC25,
	MS_ZRTP_KEY_AGREEMENT_EC38,
	MS_ZRTP_KEY_AGREEMENT_EC52,
	MS_ZRTP_KEY_AGREEMENT_X255,
	MS_ZRTP_KEY_AGREEMENT_X448
} MSZrtpKeyAgreement;

typedef enum _MSZrtpSasType{
	MS_ZRTP_SAS_INVALID,
	MS_ZRTP_SAS_B32,
	MS_ZRTP_SAS_B256
} MSZrtpSasType;

typedef struct MSZrtpParams {
	void *zidCacheDB; /**< a pointer to an sqlite database holding all zrtp related information */
	const char *selfUri; /* our sip URI, needed for zrtp Cache */
	const char *peerUri; /* the sip URI of correspondant, needed for zrtp Cache */
	uint32_t limeKeyTimeSpan; /**< amount in seconds of the lime key life span, set to 0 for infinite life span **/

	/* activated crypto types */
	MSZrtpHash             hashes[MS_MAX_ZRTP_CRYPTO_TYPES];
	MsZrtpCryptoTypesCount hashesCount ;
	MSZrtpCipher           ciphers[MS_MAX_ZRTP_CRYPTO_TYPES];
	MsZrtpCryptoTypesCount ciphersCount;
	MSZrtpAuthTag          authTags[MS_MAX_ZRTP_CRYPTO_TYPES];
	MsZrtpCryptoTypesCount authTagsCount;
	MSZrtpKeyAgreement     keyAgreements[MS_MAX_ZRTP_CRYPTO_TYPES];
	MsZrtpCryptoTypesCount keyAgreementsCount;
	MSZrtpSasType          sasTypes[MS_MAX_ZRTP_CRYPTO_TYPES];
	MsZrtpCryptoTypesCount sasTypesCount;
} MSZrtpParams;

typedef struct _MSZrtpContext MSZrtpContext ;

/**
 * check if ZRTP is available
 * @return TRUE if it is available, FALSE if not
 */
MS2_PUBLIC bool_t ms_zrtp_available(void);

/**
 * Create an initialise a ZRTP context
 * @param[in]	stream_sessions		A link to the stream sessions structures, used to get rtp session to add transport modifier and needed to set SRTP sessions when keys are ready
 * @param[in]	params			ZID cache filename and peer sip uri
 * @return	a pointer to the opaque context structure needed by MSZRTP
 */
MS2_PUBLIC MSZrtpContext* ms_zrtp_context_new(struct _MSMediaStreamSessions *stream_sessions, MSZrtpParams *params);

/**
 * Create an initialise a ZRTP context on a channel when a ZRTP exchange was already performed on an other one
 * @param[in]	stream_sessions		A link to the stream sessions structures, used to get rtp session to add transport modifier and needed to set SRTP sessions when keys are ready
 * @param[in]	activeContext		The MSZRTP context of the already active session, used to pass to lib bzrtp its own context which shall remain unique.
 * @return	a pointer to the opaque context structure needed by MSZRTP
 */
MS2_PUBLIC MSZrtpContext* ms_zrtp_multistream_new(struct _MSMediaStreamSessions *stream_sessions, MSZrtpContext* activeContext);

/***
 * Start a previously created ZRTP channel, ZRTP engine will start sending Hello packets
 * @param[in]	ctx		Context previously created using ms_zrtp_context_new or ms_zrtp_multistream_new
 * @return 0 on success
 */
MS2_PUBLIC int ms_zrtp_channel_start(MSZrtpContext *ctx);

/**
 * Free ressources used by ZRTP context
 * it will also free the libbzrtp context if no more channel are active
 * @param[in/out]	context		the opaque MSZRTP context
 */
MS2_PUBLIC void ms_zrtp_context_destroy(MSZrtpContext *ctx);

/**
 * can be used to give more time for establishing zrtp session
 * @param[in] ctx	The MSZRTP context
 * */
MS2_PUBLIC void ms_zrtp_reset_transmition_timer(MSZrtpContext* ctx);

/**
 * Tell the MSZRTP context that SAS was controlled by user, it will trigger a ZID cache update
 * @param[in]	ctx	MSZRTP context, used to retrieve cache and update it
 */
MS2_PUBLIC void ms_zrtp_sas_verified(MSZrtpContext* ctx);

/**
 * Tell the MSZRTP context that user have requested the SAS verified status to be reseted, it will trigger a ZID cache update
 * @param[in]	ctx	MSZRTP context, used to retrieve cache and update it
 */
MS2_PUBLIC void ms_zrtp_sas_reset_verified(MSZrtpContext* ctx);

/**
 * Get the ZRTP Hello Hash from the given context
 * @param[in]	ctx	MSZRTP context
 * @param[out]	The Zrtp Hello Hash as defined in RFC6189 section 8
 */
MS2_PUBLIC int ms_zrtp_getHelloHash(MSZrtpContext* ctx, uint8_t *output, size_t outputLength);

/**
 * Set the peer ZRTP Hello Hash to the given context
 * @param[in]	ctx	MSZRTP context
 * @param[in]	The Zrtp Hello Hash as defined in RFC6189 section 8
 * @param[in]	The Zrtp Hello Hash length
 *
 * @return 0 on succes, Error code otherwise
 */
MS2_PUBLIC int ms_zrtp_setPeerHelloHash(MSZrtpContext *ctx, uint8_t *peerHelloHashHexString, size_t peerHelloHashHexStringLength);

/**
 * from_string and to_string for enums: MSZrtpHash, MSZrtpCipher, MSZrtpAuthTag, MSZrtpKeyAgreement, MSZrtpSasType
 */
MS2_PUBLIC MSZrtpHash ms_zrtp_hash_from_string(const char* str);
MS2_PUBLIC const char* ms_zrtp_hash_to_string(const MSZrtpHash hash);
MS2_PUBLIC MSZrtpCipher ms_zrtp_cipher_from_string(const char* str);
MS2_PUBLIC const char* ms_zrtp_cipher_to_string(const MSZrtpCipher cipher);
MS2_PUBLIC MSZrtpAuthTag ms_zrtp_auth_tag_from_string(const char* str);
MS2_PUBLIC const char* ms_zrtp_auth_tag_to_string(const MSZrtpAuthTag authTag);
MS2_PUBLIC MSZrtpKeyAgreement ms_zrtp_key_agreement_from_string(const char* str);
MS2_PUBLIC const char* ms_zrtp_key_agreement_to_string(const MSZrtpKeyAgreement keyAgreement);
MS2_PUBLIC MSZrtpSasType ms_zrtp_sas_type_from_string(const char* str);
MS2_PUBLIC const char* ms_zrtp_sas_type_to_string(const MSZrtpSasType sasType);

/* Cache wrapper functions : functions needed by liblinphone wrapped to avoid direct dependence of linphone on bzrtp */
/**
 * @brief Check the given sqlite3 DB and create requested tables if needed
 * 	Also manage DB schema upgrade
 * @param[in/out]	db	Pointer to the sqlite3 db open connection
 * 				Use a void * to keep this API when building cacheless
 *
 * @return 0 on succes, MSZRTP_CACHE_SETUP if cache was empty, MSZRTP_CACHE_UPDATE if db structure was updated error code otherwise
 */
MS2_PUBLIC int ms_zrtp_initCache(void *db);

/**
 * @brief Perform migration from xml version to sqlite3 version of cache
 *	Warning: new version of cache associate a ZID to each local URI, the old one did not
 *		the migration function will associate any data in the cache to the sip URI given in parameter which shall be the default URI
 * @param[in]		cacheXml	a pointer to an xmlDocPtr structure containing the old cache to be migrated
 * @param[in/out]	cacheSqlite	a pointer to an sqlite3 structure containing a cache initialised using ms_zrtp_cache_init function
 * @param[in]		selfURI		default sip URI for this end point, NULL terminated char
 *
 * @return	0 on success, MSZRTP_ERROR_CACHEDISABLED when bzrtp was not compiled with cache enabled, MSZRTP_ERROR_CACHEMIGRATIONFAILED on error during migration
 */
MS2_PUBLIC int ms_zrtp_cache_migration(void *cacheXmlPtr, void *cacheSqlite, const char *selfURI);

#ifdef __cplusplus
}
#endif

#endif /* ms_zrtp_h */
