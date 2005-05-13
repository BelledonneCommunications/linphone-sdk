/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  Copyright (C) 2004, 2005  Anil Kumar, Jerry St.Clair
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
 *  Contains Type Definitions for some generic functions used across
 *  CUnit project files.
 *
 *  Created By     : Anil Kumar on 13/Oct/2001
 *  Last Modified  : 13/Oct/2001
 *  Comment        : Moved some of the generic functions declarations
 *                   from other files to this one so as to use the
 *                   functions consitently. This file is not included
 *                   in the distribution headers because it is used
 *                   internally by CUnit.
 *  EMail          : aksaharan@yahoo.com
 *
 *  Last Modified  : 20-Jul-2004 (JDS)
 *  Comment        : New interface, support for deprecated version 1 names.
 *  EMail          : jds2@users.sourceforge.net
 *
 *  Modified       : 5-Sep-2004 (JDS)
 *  Comment        : Added internal test interface.
 *  EMail          : jds2@users.sourceforge.net
 */

/** @file
 *  Utility functions (user interface).
 */
/** @addtogroup Framework
 * @{
 */

#ifndef _CUNIT_UTIL_H
#define _CUNIT_UTIL_H

#include "CUnit.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum string length. */
#define CUNIT_MAX_STRING_LENGTH	1024
/** maximum number of characters in a translated xml entity. */
#define CUNIT_MAX_ENTITY_LEN 5

CU_EXPORT int CU_translate_special_characters(const char* szSrc, char* szDest, int maxlen);
CU_EXPORT int CU_compare_strings(const char* szSrc, const char* szDest);

CU_EXPORT void CU_trim_left(char* szString);
CU_EXPORT void CU_trim_right(char* szString);
CU_EXPORT void CU_trim(char* szString);

#ifdef CUNIT_BUILD_TESTS
void test_cunit_Util(void);
#endif

#ifdef __cplusplus
}
#endif

#ifdef USE_DEPRECATED_CUNIT_NAMES
/** Deprecated (version 1). @deprecated Use CU_translate_special_characters(). */
#define translate_special_characters(src, dest, len) CU_translate_special_characters(src, dest, len)
/** Deprecated (version 1). @deprecated Use CU_compare_strings(). */
#define compare_strings(src, dest) CU_compare_strings(src, dest)

/** Deprecated (version 1). @deprecated Use CU_trim_left(). */
#define trim_left(str) CU_trim_left(str)
/** Deprecated (version 1). @deprecated Use CU_trim_right(). */
#define trim_right(str) CU_trim_right(str)
/** Deprecated (version 1). @deprecated Use CU_trim(). */
#define trim(str) CU_trim(str)

#endif  /* USE_DEPRECATED_CUNIT_NAMES */

#endif /* _CUNIT_UTIL_H */
/** @} */
