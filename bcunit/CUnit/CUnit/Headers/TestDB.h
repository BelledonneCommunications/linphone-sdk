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
 *	Contains all the Type Definitions and functions declarations
 *	for the CUnit test database maintenance.
 *
 *	Created By     : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified  : 09/Aug/2001
 *	Comment        : Added Preprocessor conditionals for the file.
 *	EMail          : aksaharan@yahoo.com
 *
 *	Last Modified  : 24/aug/2001 by Anil Kumar
 *	Comment        : Made the linked list from SLL to DLL(doubly linked list).
 *	EMail          : aksaharan@yahoo.com
 *
 */

#ifndef _CUNIT_TESTDB_H
#define _CUNIT_TESTDB_H 1

#include "CUnit.h"
#include "Errno.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	Type definition for Initialization/Cleaup/TestFunction for TestCase/TestGroup
 */
typedef int (*InitializeFunc)(void);
typedef int (*CleanupFunc)(void);
typedef void (*TestFunc)(void);

typedef struct _TestCase
{
	char*			pName;
	TestFunc		pTestFunc;

	struct _TestCase*		pNext;
	struct _TestCase*		pPrev;

}TestCase;
typedef TestCase *PTestCase;

typedef struct _TestGroup
{
	char*			pName;
	PTestCase		pTestCase;
	InitializeFunc	pInitializeFunc;
	CleanupFunc		pCleanupFunc;

	unsigned int	uiNumberOfTests;
	struct _TestGroup*		pNext;
	struct _TestGroup*		pPrev;

}TestGroup;
typedef TestGroup *PTestGroup;

typedef struct _TestResult
{
	unsigned int	uiLineNumber;
	char*			strFileName;
	char*			strCondition;	
	PTestCase		pTestCase;
	PTestGroup		pTestGroup;

	struct _TestResult*		pNext;
	struct _TestResult*		pPrev;

}TestResult;
typedef TestResult *PTestResult;

typedef struct _TestRegistry
{
	unsigned int	uiNumberOfGroups;
	unsigned int	uiNumberOfTests;
	unsigned int	uiNumberOfFailures;

	PTestGroup		pGroup;
	PTestResult		pResult;
}TestRegistry;
typedef TestRegistry *PTestRegistry;

extern PTestRegistry g_pTestRegistry;
extern int error_number;

extern int initialize_registry(void);
extern void cleanup_registry(void);

extern PTestRegistry get_registry(void);
extern int set_registry(PTestRegistry pTestRegistry);


extern PTestGroup add_test_group(char* strName, InitializeFunc pInit, CleanupFunc pClean);
extern PTestCase add_test_case(PTestGroup pGroup, char* strName, TestFunc pTest);
/*
 * This function is for internal use and is used by the 
 * Asssert Implementation function to store the error description
 * and the codes.
 */
extern void add_failure(unsigned int uiLineNumber, char szCondition[],
				char szFileName[], PTestGroup pGroup, PTestCase pTest);

extern const char* get_error(void);

#ifdef __cplusplus
}
#endif
#endif  /*  _CUNIT_TESTDB_H  */
