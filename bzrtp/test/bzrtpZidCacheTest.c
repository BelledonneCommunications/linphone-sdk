/*
 * Copyright (c) 2014-2023 Belledonne Communications SARL.
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

#include "bzrtp/bzrtp.h"
#include "testUtils.h"
#include "zidCache.h"
#include "bzrtpTest.h"

#ifdef ZIDCACHE_ENABLED
#include "sqlite3.h"
#endif /* ZIDCACHE_ENABLED */



static void test_cache_getSelfZID(void) {
#ifdef ZIDCACHE_ENABLED
	bzrtpContext_t *aliceContext;
	sqlite3 *aliceDB=NULL;
	uint8_t selfZIDalice[12];
	uint8_t selfZIDecila[12];
	char patternFilename[1024];
	uint8_t patternZIDalice[12] = {0x42, 0x31, 0xf2, 0x9a, 0x6b, 0x14, 0xbc, 0xdf, 0x69, 0x72, 0x9f, 0xb8};
	uint8_t patternZIDecila[12] = {0x17, 0x72, 0xae, 0x6b, 0x57, 0xb0, 0x3d, 0x99, 0x96, 0x3d, 0x63, 0xb2};
	uint8_t patternZIDbob[12] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xed, 0xcb, 0xa9, 0x87};
	char *resource_dir = (char *)bc_tester_get_resource_dir_prefix();
	int ret,zuidalicebob=0, zuidCheck=0;
	/* write/read patterns: write rs1, rs2 and pvs, read also aux*/
	int patternLength = 3;
	uint8_t rs1Value[] = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};
	uint8_t rs2Value[] = {0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xff, 0xee};
	uint8_t pvsValue[] = {0x1};
	const char *patternColNames[]={"rs1", "rs2", "pvs", "aux"};
	uint8_t *patternColValues[] = {rs1Value, rs2Value, pvsValue};
	size_t patternColValuesLength[] = {16, 16, 1};
	uint8_t *readValues[] = {NULL, NULL, NULL,NULL};
	size_t readLength[4];
	int i;
	char *aliceCacheFile = bc_tester_file("tmpZIDAlice.sqlite");;

	/* we need a bzrtp context */
	aliceContext = bzrtp_createBzrtpContext();

	/* create a new DB file */
	remove(aliceCacheFile);
	bzrtptester_sqlite3_open(aliceCacheFile, &aliceDB);
	/* check what happend if we try to run cacheless */
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, NULL, "alice@sip.linphone.org", "bob@sip.linphone.org"),BZRTP_ZIDCACHE_RUNTIME_CACHELESS, int, "%x");
	/* add real cache data into context using the dedicated function */
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, (void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org"), BZRTP_CACHE_SETUP,int,"%x");
	sqlite3_close(aliceDB); /* close the DB and reopen/init it, check the return value is now 0*/
	bzrtptester_sqlite3_open(aliceCacheFile, &aliceDB);
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, (void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org"), 0,int,"%x");
	/* call to get selfZID, it will create the ZID for alice@sip.linphone.org. */
	/* Note: in regular use we must call bzrtp_initBzrtpContext but here we are just testing the getSelfZID */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock(aliceContext->zidCache, aliceContext->selfURI, aliceContext->selfZID, aliceContext->RNGContext, NULL), 0, int, "%x");
	/* get it again, in another buffer and compare */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock(aliceDB, "alice@sip.linphone.org", selfZIDalice, aliceContext->RNGContext, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDalice, aliceContext->selfZID, 12), 0, int, "%d");

	/* fetch a ZID for an other adress on the same cache, it shall create a new one */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock(aliceDB, "ecila@sip.linphone.org", aliceContext->selfZID, aliceContext->RNGContext, NULL), 0, int, "%x");
	BC_ASSERT_NOT_EQUAL(memcmp(selfZIDalice, aliceContext->selfZID,12), 0, int, "%d"); /* check it is a different one */
	/* fetch it again and compare */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock(aliceDB, "ecila@sip.linphone.org", selfZIDecila, aliceContext->RNGContext, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDecila, aliceContext->selfZID, 12), 0, int, "%d");
	/* fetch again the first one and compare */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock(aliceDB, "alice@sip.linphone.org", aliceContext->selfZID, aliceContext->RNGContext, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDalice, aliceContext->selfZID, 12), 0, int, "%d");

	/* try to get a zuid on alice/bob+patternZIDbob, at first the row shall be created */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", patternZIDbob, BZRTP_ZIDCACHE_INSERT_ZUID, &zuidalicebob, NULL), 0, int, "%x");
	/* ask for it again and check they are the same */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", patternZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidCheck, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(zuidalicebob, zuidCheck, int, "%d");

	/* Then write in cache zrtp table */
	BC_ASSERT_EQUAL(bzrtp_cache_write_lock((void *)aliceDB, zuidalicebob, "zrtp", patternColNames, (uint8_t **)patternColValues, patternColValuesLength, patternLength, NULL), 0, int, "%x");
	/* Try to write a zuid row in zrtp table while zuid is not present in ziduri table: it shall fail */
	BC_ASSERT_EQUAL(bzrtp_cache_write_lock((void *)aliceDB, zuidalicebob+10, "zrtp", patternColNames, (uint8_t **)patternColValues, patternColValuesLength, patternLength, NULL), BZRTP_ZIDCACHE_UNABLETOUPDATE, int, "%x");
	/* Now read the data and check they're the same */
	BC_ASSERT_EQUAL(bzrtp_cache_read_lock((void *)aliceDB, zuidalicebob, "zrtp", patternColNames, readValues, readLength, patternLength+1, NULL), 0, int, "%x");
	for (i=0; i<patternLength; i++) {
		BC_ASSERT_EQUAL(readLength[i], patternColValuesLength[i], size_t, "%zu");
		BC_ASSERT_EQUAL(memcmp(readValues[i], patternColValues[i], patternColValuesLength[i]), 0, int, "%d");
	}
	/* we also read aux columns wich shall be NULL */
	BC_ASSERT_PTR_NULL(readValues[patternLength]);
	BC_ASSERT_EQUAL(readLength[patternLength], 0, size_t, "%zu");

	sqlite3_close(aliceDB);

	remove(aliceCacheFile);
	bc_free(aliceCacheFile);

	/* read ZIDs from pattern file and check they match expected */
	sprintf(patternFilename, "%s/patternZIDAlice.sqlite", resource_dir);
	BC_ASSERT_EQUAL((ret = bzrtptester_sqlite3_open(patternFilename, &aliceDB)), SQLITE_OK, int, "0x%x");
	if (ret != SQLITE_OK) {
		bzrtp_message("Error: unable to find patternZIDAlice.sqlite file. Did you set correctly the --resource-dir argument(current set: %s)", resource_dir==NULL?"NULL":resource_dir);
		return;
	}

	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock(aliceDB, "alice@sip.linphone.org", selfZIDalice, aliceContext->RNGContext, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDalice, patternZIDalice, 12), 0, int, "%x");

	/* test the getZuid function */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", patternZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidalicebob, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(zuidalicebob, 5, int, "%d"); /* from the pattern DB: the zuid for alice/bob+provided ZID is 5 */

	BC_ASSERT_EQUAL(bzrtp_getSelfZID_lock(aliceDB, "ecila@sip.linphone.org", selfZIDecila, aliceContext->RNGContext, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDecila, patternZIDecila, 12), 0, int, "%x");

	bzrtp_destroyBzrtpContext(aliceContext, 0); /* note: we didn't initialised any channel, so just give 0 to destroy, it will destroy the bzrtp context itself */

	for (i=0; i<4; i++) {
		free(readValues[i]);
	}

	sqlite3_close(aliceDB);

#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif
}

static void test_cache_zrtpSecrets(void) {
#ifdef ZIDCACHE_ENABLED
	bzrtpContext_t *aliceContext;
	sqlite3 *aliceDB=NULL;
	uint8_t peerZIDbob[12] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0xed, 0xcb, 0xa9, 0x87,};
	uint8_t patternRs1[16] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0x12};
	uint8_t patternRs2[32] = {0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55};
	uint8_t patternAux[27] = {0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x6e, 0x20, 0x61, 0x75, 0x78, 0x69, 0x6c, 0x69, 0x61, 0x72, 0x79, 0x20, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74};
	char patternFilename[1024];
	char *resource_dir = (char *)bc_tester_get_resource_dir_prefix();
	int ret;

	/* we need a bzrtp context */
	aliceContext = bzrtp_createBzrtpContext();

	/* open the pattern file and set it in the zrtp context as zidcache db */
	sprintf(patternFilename, "%s/patternZIDAlice.sqlite", resource_dir);
	BC_ASSERT_EQUAL((ret = bzrtptester_sqlite3_open(patternFilename, &aliceDB)), SQLITE_OK, int, "0x%x");
	if (ret != SQLITE_OK) {
		bzrtp_message("Error: unable to find patternZIDAlice.sqlite file. Did you set correctly the --resource-dir argument(current set: %s)", resource_dir==NULL?"NULL":resource_dir);
		return;
	}
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, (void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org"),0,int,"%x");

	/* get Secrets */
	BC_ASSERT_EQUAL(bzrtp_getPeerAssociatedSecrets(aliceContext, peerZIDbob), 0, int, "%x");

	BC_ASSERT_EQUAL(aliceContext->zuid, 5, int, "%d");
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.rs1Length, 16, int, "%d");
	BC_ASSERT_EQUAL(memcmp(aliceContext->cachedSecret.rs1,patternRs1, 16), 0, int, "%d");
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.rs2Length, 32, int, "%d");
	BC_ASSERT_EQUAL(memcmp(aliceContext->cachedSecret.rs2,patternRs2, 32), 0, int, "%d");
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.auxsecretLength, 27, size_t, "%zu");
	BC_ASSERT_EQUAL(memcmp(aliceContext->cachedSecret.auxsecret,patternAux, 27), 0, int, "%d");
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.pbxsecretLength, 0, size_t, "%zu");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.pbxsecret);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.previouslyVerifiedSas, 1, int, "%d");

	/* now try to retrieve secret for an unknow peer uri */
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, (void *)aliceDB, "alice@sip.linphone.org", "eve@sip.linphone.org"),0,int,"%x");
	BC_ASSERT_EQUAL(bzrtp_getPeerAssociatedSecrets(aliceContext, peerZIDbob), 0, int, "%x");
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.rs1Length, 0, int, "%d");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.rs1);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.rs2Length, 0, int, "%d");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.rs2);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.auxsecretLength, 0, size_t, "%zu");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.auxsecret);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.pbxsecretLength, 0, size_t, "%zu");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.pbxsecret);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.previouslyVerifiedSas, 0, int, "%d");

	bzrtp_destroyBzrtpContext(aliceContext, 0); /* note: we didn't initialised any channel, so just give 0 to destroy, it will destroy the bzrtp context itself */
	sqlite3_close(aliceDB);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}

static test_t zidcache_tests[] = {
	TEST_NO_TAG("SelfZID", test_cache_getSelfZID),
	TEST_NO_TAG("ZRTP secrets", test_cache_zrtpSecrets),
};

test_suite_t zidcache_test_suite = {
	"ZID Cache",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(zidcache_tests) / sizeof(zidcache_tests[0]),
	zidcache_tests,
	0
};
