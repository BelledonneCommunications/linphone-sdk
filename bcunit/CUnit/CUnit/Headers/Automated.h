/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  Copyright (C) 2004  Anil Kumar, Jerry St.Clair
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
 *
 *	Modified        : 20-Jul-2004 (JDS)
 *	Comment         : New interface, doxygen comments.
 *	EMail           : jds2@users.sourceforge.net
 */

/** @file
 * Automated testing interface with xml output (user interface).
 */
/** @addtogroup Automated
 * @{
 */

#ifndef _CUNIT_AUTOMATED_H
#define _CUNIT_AUTOMATED_H

#include "CUnit.h"
#include "TestDB.h"

#ifdef __cplusplus
extern "C" {
#endif

void         CU_automated_run_tests(void);
CU_ErrorCode CU_list_tests_to_file(void);
void         CU_set_output_filename(const char* szFilenameRoot);

#ifdef USE_DEPRECATED_CUNIT_NAMES
/** Deprecated (version 1). @deprecated Use CU_automated_run_tests(). */
#define automated_run_tests() CU_automated_run_tests()
/** Deprecated (version 1). @deprecated Use CU_set_output_filename(). */
#define set_output_filename(x) CU_set_output_filename((x))
#endif  /* USE_DEPRECATED_CUNIT_NAMES */

#ifdef __cplusplus
}
#endif
#endif  /*  _CUNIT_AUTOMATED_H_  */
/** @} */
