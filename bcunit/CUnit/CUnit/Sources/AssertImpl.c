/*
 *	Contains ASSERT implementation which handles the assert.
 *	If logs if the Condition is false else it ignores it.
 *
 *	Created By     : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified  : 09/Aug/2001
 *	Comment	       : Added only skeleton function.
 *	Email          : aksaharan@yahoo.com
 *
 *	Last Modified  : 19/Aug/2001
 *	Comment	       : Added add_failure routine call.
 *	Email          : aksaharan@yahoo.com
 *
 */

#include <stdio.h>

#include "TestDB.h"
#include "TestRun.h"


void assertImplementation(unsigned int uiValue, unsigned int uiLine,
		char strCondition[], char strFile[], char strFunction[])
{
	add_failure(uiLine, strCondition, strFile, g_pTestGroup, g_pTestCase);
}
