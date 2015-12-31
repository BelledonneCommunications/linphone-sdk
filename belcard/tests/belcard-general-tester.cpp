/*
	belcard-general-tester.cpp
	Copyright (C) 2015  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belcard/belcard.hpp"
#include "belcard-tester.hpp"

using namespace::std;
using namespace::belcard;

static void kind_property(void) {
	test_property<BelCardKind>("KIND:individual\r\n");
}

static void source_property(void) {
	test_property<BelCardSource>("SOURCE:ldap://ldap.example.com/cn=Babs%20Jensen,%20o=Babsco,%20c=US\r\n");
	test_property<BelCardSource>("SOURCE:http://directory.example.com/addressbooks/jdoe/Jean%20Dupont.vcf\r\n");
}

static void xml_property(void) {
	test_property<BelCardXML>("XML:<config xmlns=\"http://www.linphone.org/xsds/lpconfig.xsd\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"http://www.linphone.org/xsds/lpconfig.xsd lpconfig.xsd\"></config>\r\n");
}

static test_t tests[] = {
	{ "Kind", kind_property },
	{ "Source", source_property },
	{ "XML", xml_property },
};

test_suite_t vcard_general_properties_test_suite = {
	"General",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};