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
 *	Contains CUnit error codes which can be used externally.
 *
 *	Created By       : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified    : 09/Aug/2001
 *	Comment          : -------
 *	EMail            : aksaharan@yahoo.com
 *
 *	Modified         : 02/Oct/2001
 *	Comment          : Added proper Eror Codes
 *	EMail            : aksaharan@yahoo.com
 *	
 *	Modified         : 13/Oct/2001
 *	Comment          : Added Eror Codes for Duplicate TestGroup and Test
 *	EMail            : aksaharan@yahoo.com
 *
 */

#ifndef _CUNIT_ERRNO_H
#define _CUNIT_ERRNO_H 1

#include <errno.h>

#define CUE_SUCCESS		0
#define CUE_NOMEMORY	1

/* Test Registry Level Errors */
#define CUE_NOREGISTRY		10
#define CUE_REGISTRY_EXISTS	11

/* Test Group Level Errors */
#define CUE_NOGROUP			20
#define CUE_NO_GROUPNAME	21
#define CUE_GRPINIT_FAILED	22
#define CUE_GRPCLEAN_FAILED	23
#define CUE_DUP_GROUP		24

/* Test Case Level Errors */
#define CUE_NOTEST			30
#define CUE_NO_TESTNAME		31
#define CUE_DUP_TEST		32

#endif  /*  _CUNIT_ERRNO_H  */
