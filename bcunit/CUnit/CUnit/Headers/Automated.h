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
 *	Contains Interface for Automated Run tests which generates HTML Report Files.
 *
 *	Created By      : Anil Kumar on ...(in month of Feb 2002)
 *	Last Modified   : 13/Feb/2002
 *	Comment         : Single interface to automated_run_tests
 *	EMail           : aksaharan@yahoo.com
 */

#ifndef _CUNIT_AUTOMATED_H
#define _CUNIT_AUTOMATED_H 1

#include "CUnit.h"
#include "TestDB.h"

extern void automated_run_tests(void);
extern void set_output_filename(char* szFilename);

#endif  /*  _CUNIT_AUTOMATED_H_  */
