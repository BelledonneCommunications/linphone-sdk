/**
 @file bzrtpTests.c

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

#include <stdio.h>
#include "CUnit/Basic.h"
#include "bzrtpCryptoTest.h"
#include "bzrtpParserTest.h"
#include "typedef.h"

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#endif

	
int main(int argc, char *argv[] ) {
	CU_pSuite cryptoWrapperTestSuite, cryptoUtilsTestSuite, parserTestSuite;
	
#ifdef HAVE_LIBXML2
	xmlInitParser();
#endif
	
	/* initialize the CUnit test registry */
	if (CUE_SUCCESS != CU_initialize_registry()) {
		return CU_get_error();
	}

	/* Add the cryptoWrapper suite to the registry */
	cryptoWrapperTestSuite = CU_add_suite("Bzrtp Crypto Wrappers", NULL, NULL);
	CU_add_test(cryptoWrapperTestSuite, "RNG", test_RNG);
	CU_add_test(cryptoWrapperTestSuite, "SHA256", test_sha256);
	CU_add_test(cryptoWrapperTestSuite, "HMAC-SHA256", test_hmacSha256);
	CU_add_test(cryptoWrapperTestSuite, "AES128-CFB", test_aes128CFB);
	CU_add_test(cryptoWrapperTestSuite, "DHM2048", test_dhm2048);
	CU_add_test(cryptoWrapperTestSuite, "DHM3072", test_dhm3072);


	/* Add the cryptoUtils suite to the registry */
	cryptoUtilsTestSuite = CU_add_suite("Bzrtp Crypto Utils", NULL, NULL);
	CU_add_test(cryptoUtilsTestSuite, "zrtpKDF", test_zrtpKDF);
	CU_add_test(cryptoUtilsTestSuite, "CRC32", test_CRC32);
	CU_add_test(cryptoUtilsTestSuite, "algo agreement", test_algoAgreement);
	CU_add_test(cryptoUtilsTestSuite, "context algo setter and getter", test_algoSetterGetter);

	/* Add the parser suite to the registry */
	parserTestSuite = CU_add_suite("Bzrtp ZRTP Packet Parser", NULL, NULL);
/*	CU_add_test(parserTestSuite, "Parse", test_parser);*/
/*	CU_add_test(parserTestSuite, "Parse Exchange", test_parserComplete);*/
	CU_add_test(parserTestSuite, "State machine", test_stateMachine);

	/* Run all suites */
	printf("\n\n#### Run the Bzrtp Crypto Wrappers tests suite\n");
	CU_basic_run_suite(cryptoWrapperTestSuite);
	printf("\n\n#### Run the Bzrtp Crypto Utils tests suite\n");
	CU_basic_run_suite(cryptoUtilsTestSuite);
	printf("\n\n#### Run the Bzrtp ZRTP Packet Parser tests suite\n");
	CU_basic_run_suite(parserTestSuite);


	/* cleanup the CUnit registry */
	CU_cleanup_registry();

#ifdef HAVE_LIBXML2
	/* cleanup libxml2 */
	xmlCleanupParser();
#endif

	return 0;
}

