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
 */

#ifndef _CUNIT_CUNIT_H
#define _CUNIT_CUNIT_H 1

#include "Errno.h"
	
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
	#define FALSE	(int)0
#endif

#ifndef TRUE
	#define TRUE	(int)~FALSE
#endif

extern void assertImplementation(unsigned int bValue,unsigned int uiLine,
				char strCondition[], char strFile[], char strFunction[]);


#undef ASSERT
#define ASSERT(value) if (0 == (int)(value))  {  assertImplementation(value, __LINE__, #value, __FILE__, "");  return;  }
		

#endif  /*  _CUNIT_CUNIT_H  */
