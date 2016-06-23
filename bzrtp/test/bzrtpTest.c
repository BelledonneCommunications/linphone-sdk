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
#include "bzrtpCryptoTest.h"
#include "bzrtpParserTest.h"
#include "typedef.h"
#include "testUtils.h"
#include <bctoolbox/logging.h>
#include <bctoolbox/tester.h>

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#endif


test_t crypto_utils_tests[] = {
	{ "zrtpKDF", test_zrtpKDF },
	{ "CRC32", test_CRC32 },
	{ "algo agreement", test_algoAgreement },
	{ "context algo setter and getter", test_algoSetterGetter },
	{ "adding mandatory crypto algorithms if needed", test_addMandatoryCryptoTypesIfNeeded }
};

test_suite_t crypto_utils_test_suite = {
	"Crypto Utils",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(crypto_utils_tests) / sizeof(crypto_utils_tests[0]),
	crypto_utils_tests
};

test_t packet_parser_tests[] = {
	{ "Parse", test_parser },
	{ "Parse hvi check fail", test_parser_hvi },
	{ "Parse Exchange", test_parserComplete },
	{ "State machine", test_stateMachine },
	{ "ZRTP-hash", test_zrtphash }
};

test_suite_t packet_parser_test_suite = {
	"Packet Parser",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(packet_parser_tests) / sizeof(packet_parser_tests[0]),
	packet_parser_tests
};

void bzrtp_tester_init(void) {
#ifdef HAVE_LIBXML2
	xmlInitParser();
#endif
	bc_tester_init(NULL, BCTBX_LOG_MESSAGE, BCTBX_LOG_ERROR, NULL);

	bc_tester_add_suite(&crypto_utils_test_suite);
	bc_tester_add_suite(&packet_parser_test_suite);
}

void bzrtp_tester_uninit(void) {
	bc_tester_uninit();
#ifdef HAVE_LIBXML2
	/* cleanup libxml2 */
	xmlCleanupParser();
#endif
}

int main(int argc, char *argv[]) {
	int i;
	int ret;

	bzrtp_tester_init();

	for (i = 1; i < argc; ++i) {
		int ret = bc_tester_parse_args(argc, argv, i);
		if (ret > 0) {
			i += ret - 1;
			continue;
		} else if (ret < 0) {
			bc_tester_helper(argv[0], "");
		}
		return ret;
	}

	ret = bc_tester_start(argv[0]);
	bzrtp_tester_uninit();
	return ret;
}

