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
