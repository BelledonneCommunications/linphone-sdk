/*
 *	Contains ASSERT Macro definition
 *
 *	Created By      : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified   : 09/Aug/2001
 *	Comment         : ASSERT definition
 *	EMail           : aksaharan@yahoo.com
 */

#ifndef _CUnit_h_
#define _CUnit_h_

#include "Errno.h"
#include "TestDB.h"
#include "TestRun.h"

#include "Console.h"
	
/* 
 * 	This is including the null character for termination. In effect it comes out
 * 	to be 255 characters.
 */
#define MAX_TEST_NAME_LENGTH	256
#define MAX_GROUP_NAME_LENGTH	256  

extern void assertImplementation(unsigned int bValue,unsigned int uiLine,
				char strCondition[], char strFile[], char strFunction[]);


#undef ASSERT
#define ASSERT(value) if (0 == (int)(value))  {  assertImplementation(value, __LINE__, #value, __FILE__, "");  return;  }
		

#endif  /*  _CUnit_h_  */
