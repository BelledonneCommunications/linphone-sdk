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
 *	Contains Interface for console Run tests.
 *
 *	Created By      : Anil Kumar on ...(in month of Aug 2001)
 *	Last Modified   : 09/Aug/2001
 *	Comment         : Single interface to Console_run_tests
 *	EMail           : aksaharan@yahoo.com
 */

#ifndef _CUNIT_CONSOLE_H
#define _CUNIT_CONSOLE_H 1

#include "CUnit.h"
#include "TestDB.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void console_run_tests(void);

#ifdef __cplusplus
}
#endif
#endif  /*  _CUNIT_CONSOLE_H_  */
