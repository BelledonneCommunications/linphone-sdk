/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *	Contains ASSERT Macro definition
 *
 *	Created By      : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified   : 09/Aug/2001
 *	Comment         : ASSERT definition
 *	EMail           : aksaharan@yahoo.com
 *
 *	Last Modified   : 12/Mar/2003
 *	Comment         : New Assert definitions
 *	EMail           : aksaharan@yahoo.com
 *
 *	Last Modified   : 27/Jul/2003
 *	Comment         : Modified ASSERT_XXX Macro definitions
 *	EMail           : aksaharan@yahoo.com
 */

#ifndef _CUNIT_CUNIT_H
#define _CUNIT_CUNIT_H 1

#include <string.h>
#include <math.h>

#include "Errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * 	This is including the null character for termination. In effect it comes out
 * 	to be 255 characters.
 */
#define MAX_TEST_NAME_LENGTH	256
#define MAX_GROUP_NAME_LENGTH	256 

/*
 * 	Global type Definitions to be used for boolean operators
 */
#ifndef BOOL
	#define BOOL 	int
#endif

#ifndef FALSE
	#define FALSE	((int)0)
#endif

#ifndef TRUE
	#define TRUE	(~FALSE)
#endif

extern void assertImplementation(unsigned int bValue,unsigned int uiLine,
				char strCondition[], char strFile[], char strFunction[]);


#undef ASSERT
#define ASSERT(value) { if (FALSE == (int)(value)) { assertImplementation(value, __LINE__, #value, __FILE__, ""); return; }}

/* Different ASSERT_XXXX definitions for ease of use */

#define ASSERT_TRUE(value) { if (FALSE == (value)) { assertImplementation(FALSE, __LINE__, ("ASSERT_TRUE(" #value ")"), __FILE__, ""); return; }}
#define ASSERT_FALSE(value) { if (FALSE != (value)) { assertImplementation(FALSE, __LINE__, ("ASSERT_FALSE(" #value ")"), __FILE__, ""); return; }}

#define ASSERT_EQUAL(actual, expected) { if ((actual) != (expected)) { assertImplementation(FALSE, __LINE__, ("ASSERT_EQUAL(" #actual "," #expected ")"), __FILE__, ""); return; }}
#define ASSERT_NOT_EQUAL(actual, expected) { if ((void*)(actual) == (void*)(expected)) { assertImplementation(FALSE, __LINE__, ("ASSERT_NOT_EQUAL(" #actual "," #expected ")"), __FILE__, ""); return; }}

#define ASSERT_PTR_EQUAL(actual, expected) { if ((void*)(actual) != (void*)(expected)) { assertImplementation(FALSE, __LINE__, ("ASSERT_PTR_EQUAL(" #actual "," #expected ")"), __FILE__, ""); return; }}
#define ASSERT_PTR_NOT_EQUAL(actual, expected) { if ((void*)(actual) == (void*)(expected)) { assertImplementation(FALSE, __LINE__, ("ASSERT_PTR_NOT_EQUAL(" #actual "," #expected ")"), __FILE__, ""); return; }}

#define ASSERT_PTR_NULL(value)  { if (NULL != (void*)(value)) { assertImplementation(FALSE, __LINE__, ("ASSERT_PTR_NULL(" #value")"), __FILE__, ""); return; }}
#define ASSERT_PTR_NOT_NULL(value) { if (NULL == (void*)(value)) { assertImplementation(FALSE, __LINE__, ("ASSERT_PTR_NOT_NULL(" #value")"), __FILE__, ""); return; }}

#define ASSERT_STRING_EQUAL(actual, expected) { if (strcmp((const char*)actual, (const char*)expected)) { assertImplementation(FALSE, __LINE__, ("ASSERT_STRING_EQUAL(" #actual ","  #expected ")"), __FILE__, ""); return; }}
#define ASSERT_STRING_NOT_EQUAL(actual, expected) { if (!strcmp((const char*)actual, (const char*)expected)) { assertImplementation(TRUE, __LINE__, ("ASSERT_STRING_NOT_EQUAL(" #actual ","  #expected ")"), __FILE__, ""); return; }}

#define ASSERT_NSTRING_EQUAL(actual, expected, count) { if (strncmp((const char*)actual, (const char*)expected, (size_t)count)) { assertImplementation(FALSE, __LINE__, ("ASSERT_NSTRING_EQUAL(" #actual ","  #expected "," #count ")"), __FILE__, ""); return; }}
#define ASSERT_NSTRING_NOT_EQUAL(actual, expected, count) { if (!strncmp((const char*)actual, (const char*)expected, (size_t)count)) { assertImplementation(TRUE, __LINE__, ("ASSERT_NSTRING_NOT_EQUAL(" #actual ","  #expected "," #count ")"), __FILE__, ""); return; }}

#define ASSERT_DOUBLE_EQUAL(actual, expected, granularity) { if ((fabs((double)actual - expected) > fabs((double)granularity))) { assertImplementation(FALSE, __LINE__, ("ASSERT_DOUBLE_EQUAL(" #actual ","  #expected "," #granularity ")"), __FILE__, ""); return; }}
#define ASSERT_DOUBLE_NOT_EQUAL(actual, expected, granularity) { if ((fabs((double)actual - expected) <= fabs((double)granularity))) { assertImplementation(TRUE, __LINE__, ("ASSERT_DOUBLE_NOT_EQUAL(" #actual ","  #expected "," #granularity ")"), __FILE__, ""); return; }}

#ifdef __cplusplus
}
#endif

#endif  /*  _CUNIT_CUNIT_H  */
