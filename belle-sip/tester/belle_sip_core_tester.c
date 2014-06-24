/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2010  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include "CUnit/Basic.h"
#include "belle-sip/belle-sip.h"
#include "belle-sip/object.h"
#include "belle-sip/dict.h"
#include "belle_sip_tester.h"
#include "belle_sip_internal.h"

#include "register_tester.h"

#ifndef WIN32
#include <sys/types.h>
#include <inttypes.h>
#endif

#define INT_TO_VOIDPTR(i) ((void*)(intptr_t)(i))
#define VOIDPTR_TO_INT(p) ((int)(intptr_t)(p))

static int foreach_called = 0;
static int destroy_called = 0;
static int clone_called = 0;

static void test_object_data_string_destroy(void* data){
	belle_sip_free(data);
	destroy_called++;
}

static void* test_object_data_string_clone(const char*name, void* data){
	clone_called++;
	if( strcmp(name, "test_str") == 0)
		return belle_sip_strdup(data);
	else
		return data;
}

static void test_object_data_foreach_cb(const char*name, void*data, void*udata)
{
	(void)name;
	(void)data;
	(void)udata;
	foreach_called++;
}

static void test_object_data(void)
{
	belle_sip_object_t*    obj = belle_sip_object_new(belle_sip_object_t);
	belle_sip_object_t* cloned = belle_sip_object_new(belle_sip_object_t);
	int i = 123;
	const char* str = "toto", *str2 = "titi";

	/* normal insertion with no destroy callback */

	// should return 0
	CU_ASSERT_EQUAL(belle_sip_object_data_set(obj, "test_i", INT_TO_VOIDPTR(i), NULL), 0);
	// should return the value we put in it
	CU_ASSERT_EQUAL( VOIDPTR_TO_INT(belle_sip_object_data_get(obj, "test_i")), i);


	/*
	 * Overwriting insertion
	 */


	// overwrite data: should return 1 when set()
	i = 124;
	CU_ASSERT_EQUAL(belle_sip_object_data_set(obj, "test_i", INT_TO_VOIDPTR(i), NULL), 1 );
	// should return the new value we put in it
	CU_ASSERT_EQUAL( VOIDPTR_TO_INT(belle_sip_object_data_get(obj, "test_i")), i);


	/*
	 * normal insertion with destroy callback
	 */

	CU_ASSERT_EQUAL(belle_sip_object_data_set(obj, "test_str",
											  (void*)belle_sip_strdup(str),
											  test_object_data_string_destroy),
					0);

	// we should get back the same string
	CU_ASSERT_STRING_EQUAL( (const char*)belle_sip_object_data_get(obj, "test_str"),
							str );

	CU_ASSERT_EQUAL(belle_sip_object_data_remove(obj, "test_str"),0);
	// we expect the destroy() function to be called on removal
	CU_ASSERT_EQUAL(destroy_called, 1);
	destroy_called = 0;

	/*
	 * string insertion and replace
	 */
	belle_sip_object_data_set(obj, "test_str",
							  (void*)belle_sip_strdup(str),
							  test_object_data_string_destroy);
	belle_sip_object_data_set(obj, "test_str",
							  (void*)belle_sip_strdup(str2),
							  test_object_data_string_destroy);
	CU_ASSERT_EQUAL(destroy_called, 1); // we expect the dtor to have been called to free the first string


	/*
	 * Get non-existent key
	 */
	CU_ASSERT_EQUAL(belle_sip_object_data_get(obj, "non-exist"),NULL);

	/*
	 * test cloning the dictionary
	 */
	belle_sip_object_data_clone(obj, cloned, test_object_data_string_clone);
	CU_ASSERT_EQUAL(clone_called,2); // we expect the clone function to be called for "test_i" and "test_str"

	// the values should be equal
	CU_ASSERT_EQUAL( VOIDPTR_TO_INT(belle_sip_object_data_get(obj, "test_i")),
					 VOIDPTR_TO_INT(belle_sip_object_data_get(cloned, "test_i")) );

	CU_ASSERT_STRING_EQUAL( (const char*)belle_sip_object_data_get(obj, "test_str"),
							(const char*)belle_sip_object_data_get(cloned, "test_str"));
	// but the pointers should be different
	CU_ASSERT_NOT_EQUAL( (const char*)belle_sip_object_data_get(obj, "test_str"),
							(const char*)belle_sip_object_data_get(cloned, "test_str"));


	/*
	 * Foreach test
	 */
	belle_sip_object_data_foreach(obj, test_object_data_foreach_cb, NULL);
	CU_ASSERT_EQUAL( foreach_called, 2 );

	belle_sip_object_unref(obj);
	belle_sip_object_unref(cloned);

}

static void test_dictionary(void)
{
	belle_sip_dict_t* obj = belle_sip_object_new(belle_sip_dict_t);
	const char* str = "";
	int i = 5;
	int64_t i64 = 0xF2345678 << 1; // gcc doesn't like 0x1234567890 as a 64 bit literal..

	belle_sip_dict_set_int(obj, "test_i", i);
	CU_ASSERT_EQUAL(belle_sip_dict_get_int(obj,"test_i",-1),i);

	// return default int value
	CU_ASSERT_EQUAL(belle_sip_dict_get_int(obj,"unexistent",-1),-1);

	// remove existing entry
	CU_ASSERT_EQUAL(belle_sip_dict_remove(obj, "test_i"),0);

	// test_i should't be present anymore
	CU_ASSERT_EQUAL(belle_sip_dict_get_int(obj,"test_i",-1),-1);

	// remove unknown entry
	CU_ASSERT_NOT_EQUAL(belle_sip_dict_remove(obj, "unexistent"),0);

	belle_sip_dict_set_string(obj, "test_str", str);
	CU_ASSERT_STRING_EQUAL( (const char*)belle_sip_dict_get_string(obj, "test_str", ""),str);

	// unknown string value
	CU_ASSERT_STRING_EQUAL( (const char*)belle_sip_dict_get_string(obj, "unexistent", "toto"),"toto");

	belle_sip_dict_set_int64(obj, "test_i64", i64);
	CU_ASSERT_EQUAL(belle_sip_dict_get_int64(obj,"test_i64",-1),i64);

	belle_sip_dict_clear(obj);
	// test_str shouldn't exist anymore
	CU_ASSERT_STRING_EQUAL(belle_sip_dict_get_string(obj,"test_str","toto"),"toto");

	belle_sip_object_unref(obj);
}


test_t core_tests[] = {
	{ "Object Data", test_object_data },
	{ "Dictionary", test_dictionary }
};

test_suite_t core_test_suite = {
	"Core",
	NULL,
	NULL,
	sizeof(core_tests) / sizeof(core_tests[0]),
	core_tests
};

