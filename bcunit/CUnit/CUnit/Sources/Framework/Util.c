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
 *	Contains some generic functions used across CUnit project files.
 *
 *	Created By     : Anil Kumar on 13/Oct/2001
 *	Last Modified  : 13/Oct/2001
 *	Comment        : Moved some of the generic functions definitions 
 *	                 from other files to this one so as to use the 
 *	                 functions consitently. This file is not included
 *	                 in the distribution headers because it is used 
 *	                 internally by CUnit.
 *	EMail          : aksaharan@yahoo.com

 *	Last Modified  : 26/Jul/2003
 *	Comment        : Added a function to convert a string containing special
 *					 characters into escaped character for XML/HTML usage.
 *	EMail          : aksaharan@yahoo.com
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "CUnit.h"
#include "TestDB.h"
#include "Util.h"

static const struct {
	char special_char;
	char* replacement;
} bindings [] = {
		{'&', "&amp;"},
		{'>', "&gt;"},
		{'<', "&lt;"}
	};

static int get_index(char ch);

int translate_special_characters(const char* szSrc, char* szDest, int maxlen)
{
	int count = 0;
	int src = 0;
	int dest = 0;
	int length = strlen(szSrc);
	int conv_index = 0;

	memset(szDest, 0, maxlen);
	while (dest < (maxlen - 1) && src < length) {

		if (-1 != (conv_index = get_index(szSrc[src]))) {
			strcat(szDest, bindings[conv_index].replacement);
			dest += strlen(bindings[conv_index].replacement);
		} else {
			szDest[dest++] = szSrc[src];
		}

		++src;
	}

	return count;
}

int compare_strings(const char* szSrc, const char* szDest)
{
	while (*szSrc && *szDest && toupper(*szSrc) == toupper(*szDest)) {
		szSrc++;
		szDest++;
	}
		
	return *szSrc - *szDest;
}

void trim(char* szString)
{
	trim_left(szString);
	trim_right(szString);
}

void trim_left(char* szString)
{
	int nOffset = 0;
	char* szSrc = szString;
	char* szDest = szString;
	
	assert(szString);

	/* 
	 * Scan for the spaces in the starting of string.
	 */
	for (; *szSrc; szSrc++, nOffset++) {
		if (!isspace(*szSrc))
			break;
	}

	for(; nOffset && (*szDest = *szSrc); szSrc++, szDest++)
		;
	
}

void trim_right(char* szString)
{
	int nLength = 0;
	char* szSrc = szString;
	
	assert(szString);
	nLength = strlen(szString);
	/*
	 * Scan for specs in the end of string.
	 */
	for (; nLength && isspace(*(szSrc + nLength - 1)); nLength--)
		;

	*(szSrc + nLength) = '\0';
}

PTestGroup get_group_by_name(const char* szGroupName, PTestRegistry pRegistry)
{
	PTestGroup pGroup = NULL;
	PTestGroup pCur = pRegistry->pGroup;

	while (pCur) {

		if (!compare_strings(pCur->pName, szGroupName)) {
			pGroup = pCur;
			break;
		}
		pCur = pCur->pNext;
	}

	return pGroup;
}

PTestCase get_test_by_name(const char* szTestName, PTestGroup pGroup)
{
	PTestCase pTest = NULL;
	PTestCase pCur = pGroup->pTestCase;

	while (pCur) {
		
		if (!compare_strings(pCur->pName, szTestName)) {
			pTest = pCur;
			break;
		}
		pCur = pCur->pNext;
	}

	return pTest;
}

static int get_index(char ch)
{
	int length = sizeof(bindings)/sizeof(bindings[0]);
	int counter = 0;
	for (counter = 0; counter < length && bindings[counter].special_char != ch; ++counter)
		;

	return (counter < length ? counter : -1);
}
