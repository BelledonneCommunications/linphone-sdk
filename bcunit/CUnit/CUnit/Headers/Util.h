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
 *	Contains Type Definitions for some generic functions used across
 *	CUnit project files.
 *
 *	Created By     : Anil Kumar on 13/Oct/2001
 *	Last Modified  : 13/Oct/2001
 *	Comment        : Moved some of the generic functions declarations 
 *	                 from other files to this one so as to use the 
 *	                 functions consitently. This file is not included
 *	                 in the distribution headers because it is used 
 *	                 internally by CUnit.
 *	EMail          : aksaharan@yahoo.com
 *
 */

#ifndef _CUNIT_UTIL_H
#define _CUNIT_UTIL_H 1

extern int compare_strings(const char* szSrc, const char* szDest);

extern void trim_left(char* szString);
extern void trim_right(char* szString);
extern void trim(char* szString);


extern PTestGroup get_group_by_name(const char* szGroupName, PTestRegistry pRegistry);
extern PTestCase get_test_by_name(const char* szTestName, PTestGroup pGroup);

#endif /* _CUNIT_UTIL_H */
