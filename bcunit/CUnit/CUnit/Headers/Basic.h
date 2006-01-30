/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2004,2005,2006  Jerry St.Clair
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
 *  Interface for simple test runner.
 *
 *  11-Aug-2004   Initial implementation of basic test runner interface. (JDS)
 */

/** @file
 * Basic interface with output to stdout.
 */
/** @addtogroup Basic
 * @{
 */

#ifndef CUNIT_BASIC_H_SEEN
#define CUNIT_BASIC_H_SEEN

#include "CUnit.h"
#include "TestDB.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Run modes for the basic interface. */
typedef enum {
  CU_BRM_NORMAL = 0,  /**< Normal mode - failures and run summary are printed [default]. */
  CU_BRM_SILENT,      /**< Silent mode - no output is printed except framework error messages. */
  CU_BRM_VERBOSE      /**< Verbose mode - maximum output of run details. */
} CU_BasicRunMode;

CU_EXPORT CU_ErrorCode    CU_basic_run_tests(void);
CU_EXPORT CU_ErrorCode    CU_basic_run_suite(CU_pSuite pSuite);
CU_EXPORT CU_ErrorCode    CU_basic_run_test(CU_pSuite pSuite, CU_pTest pTest);
CU_EXPORT void            CU_basic_set_mode(CU_BasicRunMode mode);
CU_EXPORT CU_BasicRunMode CU_basic_get_mode(void);
CU_EXPORT void            CU_basic_show_failures(CU_pFailureRecord pFailure);

#ifdef __cplusplus
}
#endif
#endif  /*  CUNIT_BASIC_H_SEEN  */
/** @} */
