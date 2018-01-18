/**
 @file bzrtpZidCacheTest.c
 @brief Test ZID cache operation

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
#include <stdio.h>
#include <stdlib.h>

#include "bzrtp/bzrtp.h"
#include "testUtils.h"
#include "zidCache.h"
#include "bzrtpTest.h"

#ifdef ZIDCACHE_ENABLED
#include "sqlite3.h"
#ifdef HAVE_LIBXML2
#include <libxml/tree.h>
#include <libxml/parser.h>

static const char *xmlCacheMigration = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<cache><selfZID>00112233445566778899aabb</selfZID><peer><ZID>99887766554433221100ffee</ZID><rs1>c4274f13a2b6fa05c15ec93158f930e7264b0a893393376dbc80c6eb1cccdc5a</rs1><uri>sip:bob@sip.linphone.org</uri><sndKey>219d9e445d10d4ed64083c7ccbb83a23bc17a97df0af5de4261f3fe026b05b0b</sndKey><rcvKey>747e72a5cc996413cb9fa6e3d18d8b370436e274cd6ba4efc1a4580340af57ca</rcvKey><sndSId>df2bf38e719fa89e17332cf8d5e774ee70d347baa74d16dee01f306c54789869</sndSId><rcvSId>928ce78b0bfc30427a02b1b668b2b3b0496d5664d7e89b75ed292ee97e3fc850</rcvSId><sndIndex>496bcc89</sndIndex><rcvIndex>59337abe</rcvIndex><rs2>5dda11f388384b349d210612f30824268a3753a7afa52ef6df5866dca76315c4</rs2><uri>sip:bob2@sip.linphone.org</uri></peer><peer><ZID>ffeeddccbbaa987654321012</ZID><rs1>858b495dfad483af3c088f26d68c4beebc638bd44feae45aea726a771727235e</rs1><uri>sip:bob@sip.linphone.org</uri><sndKey>b6aac945057bc4466bfe9a23771c6a1b3b8d72ec3e7d8f30ed63cbc5a9479a25</sndKey><rcvKey>bea5ac3225edd0545b816f061a8190370e3ee5160e75404846a34d1580e0c263</rcvKey><sndSId>17ce70fdf12e500294bcb5f2ffef53096761bb1c912b21e972ae03a5a9f05c47</sndSId><rcvSId>7e13a20e15a517700f0be0921f74b96d4b4a0c539d5e14d5cdd8706441874ac0</rcvSId><sndIndex>75e18caa</sndIndex><rcvIndex>2cfbbf06</rcvIndex><rs2>1533dee20c8116dc2c282cae9adfea689b87bc4c6a4e18a846f12e3e7fea3959</rs2></peer><peer><ZID>0987654321fedcba5a5a5a5a</ZID><rs1>cb6ecc87d1dd87b23f225eec53a26fc541384917623e0c46abab8c0350c6929e</rs1><sndKey>92bb03988e8f0ccfefa37a55fd7c5893bea3bfbb27312f49dd9b10d0e3c15fc7</sndKey><rcvKey>2315705a5830b98f68458fcd49623144cb34a667512c4d44686aee125bb8b622</rcvKey><sndSId>94c56eea0dd829379263b6da3f6ac0a95388090f168a3568736ca0bd9f8d595f</sndSId><rcvSId>c319ae0d41183fec90afc412d42253c5b456580f7a463c111c7293623b8631f4</rcvSId><uri>sip:bob@sip.linphone.org</uri><sndIndex>2c46ddcc</sndIndex><rcvIndex>15f5779e</rcvIndex><valid>0000000058f095bf</valid><pvs>01</pvs></peer></cache>";
#endif /* HAVE_LIBXML2 */
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

	/* we need a bzrtp context */
	aliceContext = bzrtp_createBzrtpContext();

	/* create a new DB file */
	remove("tmpZIDAlice.sqlite");
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDAlice.sqlite"), &aliceDB);
	/* check what happend if we try to run cacheless */
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, NULL, "alice@sip.linphone.org", "bob@sip.linphone.org"),BZRTP_ZIDCACHE_RUNTIME_CACHELESS, int, "%x");
	/* add real cache data into context using the dedicated function */
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, (void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org"), BZRTP_CACHE_SETUP,int,"%x");
	sqlite3_close(aliceDB); /* close the DB and reopen/init it, check the return value is now 0*/
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDAlice.sqlite"), &aliceDB);
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, (void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org"), 0,int,"%x");
	/* call to get selfZID, it will create the ZID for alice@sip.linphone.org. */
	/* Note: in regular use we must call bzrtp_initBzrtpContext but here we are just testing the getSelfZID */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceContext->zidCache, aliceContext->selfURI, aliceContext->selfZID, aliceContext->RNGContext), 0, int, "%x");
	/* get it again, in another buffer and compare */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceDB, "alice@sip.linphone.org", selfZIDalice, aliceContext->RNGContext), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDalice, aliceContext->selfZID, 12), 0, int, "%d");

	/* fetch a ZID for an other adress on the same cache, it shall create a new one */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceDB, "ecila@sip.linphone.org", aliceContext->selfZID, aliceContext->RNGContext), 0, int, "%x");
	BC_ASSERT_NOT_EQUAL(memcmp(selfZIDalice, aliceContext->selfZID,12), 0, int, "%d"); /* check it is a different one */
	/* fetch it again and compare */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceDB, "ecila@sip.linphone.org", selfZIDecila, aliceContext->RNGContext), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDecila, aliceContext->selfZID, 12), 0, int, "%d");
	/* fetch again the first one and compare */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceDB, "alice@sip.linphone.org", aliceContext->selfZID, aliceContext->RNGContext), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDalice, aliceContext->selfZID, 12), 0, int, "%d");

	/* try to get a zuid on alice/bob+patternZIDbob, at first the row shall be created */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", patternZIDbob, BZRTP_ZIDCACHE_INSERT_ZUID, &zuidalicebob), 0, int, "%x");
	/* ask for it again and check they are the same */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", patternZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidCheck), 0, int, "%x");
	BC_ASSERT_EQUAL(zuidalicebob, zuidCheck, int, "%d");

	/* Then write in cache zrtp table */
	BC_ASSERT_EQUAL(bzrtp_cache_write((void *)aliceDB, zuidalicebob, "zrtp", patternColNames, (uint8_t **)patternColValues, patternColValuesLength, patternLength), 0, int, "%x");
	/* Try to write a zuid row in zrtp table while zuid is not present in ziduri table: it shall fail */
	BC_ASSERT_EQUAL(bzrtp_cache_write((void *)aliceDB, zuidalicebob+10, "zrtp", patternColNames, (uint8_t **)patternColValues, patternColValuesLength, patternLength), BZRTP_ZIDCACHE_UNABLETOUPDATE, int, "%x");
	/* Now read the data and check they're the same */
	BC_ASSERT_EQUAL(bzrtp_cache_read((void *)aliceDB, zuidalicebob, "zrtp", patternColNames, readValues, readLength, patternLength+1), 0, int, "%x");
	for (i=0; i<patternLength; i++) {
		BC_ASSERT_EQUAL(readLength[i], patternColValuesLength[i], int, "%d");
		BC_ASSERT_EQUAL(memcmp(readValues[i], patternColValues[i], patternColValuesLength[i]), 0, int, "%d");
	}
	/* we also read aux columns wich shall be NULL */
	BC_ASSERT_PTR_NULL(readValues[patternLength]);
	BC_ASSERT_EQUAL(readLength[patternLength], 0, int, "%d");

	sqlite3_close(aliceDB);

	remove("tmpZIDAlice.sqlite");

	/* read ZIDs from pattern file and check they match expected */
	sprintf(patternFilename, "%s/patternZIDAlice.sqlite", resource_dir);
	BC_ASSERT_EQUAL((ret = bzrtptester_sqlite3_open(bc_tester_file(patternFilename), &aliceDB)), SQLITE_OK, int, "0x%x");
	if (ret != SQLITE_OK) {
		bzrtp_message("Error: unable to find patternZIDAlice.sqlite file. Did you set correctly the --resource-dir argument(current set: %s)", resource_dir==NULL?"NULL":resource_dir);
		return;
	}

	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceDB, "alice@sip.linphone.org", selfZIDalice, aliceContext->RNGContext), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(selfZIDalice, patternZIDalice, 12), 0, int, "%x");

	/* test the getZuid function */
	BC_ASSERT_EQUAL(bzrtp_cache_getZuid((void *)aliceDB, "alice@sip.linphone.org", "bob@sip.linphone.org", patternZIDbob, BZRTP_ZIDCACHE_DONT_INSERT_ZUID, &zuidalicebob), 0, int, "%x");
	BC_ASSERT_EQUAL(zuidalicebob, 5, int, "%d"); /* from the pattern DB: the zuid for alice/bob+provided ZID is 5 */

	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceDB, "ecila@sip.linphone.org", selfZIDecila, aliceContext->RNGContext), 0, int, "%x");
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
	BC_ASSERT_EQUAL((ret = bzrtptester_sqlite3_open(bc_tester_file(patternFilename), &aliceDB)), SQLITE_OK, int, "0x%x");
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
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.auxsecretLength, 27, int, "%d");
	BC_ASSERT_EQUAL(memcmp(aliceContext->cachedSecret.auxsecret,patternAux, 27), 0, int, "%d");
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.pbxsecretLength, 0, int, "%d");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.pbxsecret);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.previouslyVerifiedSas, 1, int, "%d");

	/* now try to retrieve secret for an unknow peer uri */
	BC_ASSERT_EQUAL(bzrtp_setZIDCache(aliceContext, (void *)aliceDB, "alice@sip.linphone.org", "eve@sip.linphone.org"),0,int,"%x");
	BC_ASSERT_EQUAL(bzrtp_getPeerAssociatedSecrets(aliceContext, peerZIDbob), 0, int, "%x");
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.rs1Length, 0, int, "%d");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.rs1);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.rs2Length, 0, int, "%d");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.rs2);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.auxsecretLength, 0, int, "%d");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.auxsecret);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.pbxsecretLength, 0, int, "%d");
	BC_ASSERT_PTR_NULL(aliceContext->cachedSecret.pbxsecret);
	BC_ASSERT_EQUAL(aliceContext->cachedSecret.previouslyVerifiedSas, 0, int, "%d");

	bzrtp_destroyBzrtpContext(aliceContext, 0); /* note: we didn't initialised any channel, so just give 0 to destroy, it will destroy the bzrtp context itself */
	sqlite3_close(aliceDB);
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}

static void test_cache_migration(void) {
#ifdef ZIDCACHE_ENABLED
#ifdef HAVE_LIBXML2
	uint8_t pattern_selfZIDalice[12] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb};
	uint8_t selfZIDalice[12];

	sqlite3 *aliceDB=NULL;
	/* Parse the xmlCache */
	xmlDocPtr cacheXml = xmlParseDoc((xmlChar*)xmlCacheMigration);

	/* create a new DB file */
	remove("tmpZIDAlice.sqlite");
	bzrtptester_sqlite3_open(bc_tester_file("tmpZIDAlice.sqlite"), &aliceDB);
	BC_ASSERT_EQUAL(bzrtp_initCache((void *)aliceDB), BZRTP_CACHE_SETUP, int, "%x");

	/* perform migration */
	BC_ASSERT_EQUAL(bzrtp_cache_migration((void *)cacheXml, (void *)aliceDB, "sip:alice@sip.linphone.org"), 0, int, "%x");

	/* check values in new cache */
	BC_ASSERT_EQUAL(bzrtp_getSelfZID(aliceDB, "sip:alice@sip.linphone.org", selfZIDalice, NULL), 0, int, "%x");
	BC_ASSERT_EQUAL(memcmp(pattern_selfZIDalice, selfZIDalice, 12), 0, int, "%d");
	/* TODO: read values from sql cache lime and zrtp tables and check they are the expected ones */

	/* cleaning */
	sqlite3_close(aliceDB);
	xmlFree(cacheXml);
#else /* HAVE_LIBXML2 */
	bzrtp_message("Test skipped as LibXML2 not present\n");
#endif /* HAVE_LIBXML2 */
#else /* ZIDCACHE_ENABLED */
	bzrtp_message("Test skipped as ZID cache is disabled\n");
#endif /* ZIDCACHE_ENABLED */
}

static test_t zidcache_tests[] = {
	TEST_NO_TAG("SelfZID", test_cache_getSelfZID),
	TEST_NO_TAG("ZRTP secrets", test_cache_zrtpSecrets),
	TEST_NO_TAG("Migration", test_cache_migration),
};

test_suite_t zidcache_test_suite = {
	"ZID Cache",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(zidcache_tests) / sizeof(zidcache_tests[0]),
	zidcache_tests
};
