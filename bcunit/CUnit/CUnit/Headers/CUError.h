/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001            Anil Kumar
 *  Copyright (C) 2004,2005,2006  Anil Kumar, Jerry St.Clair
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
 *  Contains CUnit error codes which can be used externally.
 *
 *  Aug 2001      Initial implementation.  (AK)
 *
 *  02/Oct/2001   Added proper Eror Codes. (AK)
 *
 *  13-Oct-2001   Added Error Codes for Duplicate TestGroup and Test. (AK)
 *
 *  03-Aug-2004   Converted error code macros to an enum, doxygen comments, moved
 *                error handing code here, changed file name from Errno.h, added
 *                error codes for file open errors, added error action selection. (JDS)
 *
 *  05-Sep-2004   Added internal test interface. (JDS)
 */

/** @file
 *  Error handling functions (user interface).
 *  CUnit uses a simple (and conventional) error handling strategy.
 *  Functions that can generate errors set (and usually return) an
 *  error code to indicate the run status.  The error code can be
 *  inspected using the CU_get_error() function.  A descriptive
 *  error message can be retrieved using CU_get_error_msg().
 */
/** @addtogroup Framework
 * @{
 */

#ifndef CUNIT_CUERROR_H_SEEN
#define CUNIT_CUERROR_H_SEEN

#include <errno.h>

/*------------------------------------------------------------------------*/
/** CUnit error codes.
 *  If codes are added or removed, be sure to make a change to the
 *  error messages in CUError.c/get_error_desc().
 *  @see CU_set_error()
 *  @see CU_get_error()
 *  @see CU_get_error_msg()
 */
typedef enum {
  /* basic errors */
  CUE_SUCCESS           = 0,  /**< No error condition. */
  CUE_NOMEMORY          = 1,  /**< Memory allocation failed. */

  /* Test Registry Level Errors */
  CUE_NOREGISTRY        = 10,  /**< Test registry not initialized. */
  CUE_REGISTRY_EXISTS   = 11,  /**< Attempt to CU_set_registry() without CU_cleanup_registry(). */

  /* Test Suite Level Errors */
  CUE_NOSUITE           = 20,  /**< A required CU_pSuite pointer was NULL. */
  CUE_NO_SUITENAME      = 21,  /**< Required CU_Suite name not provided. */
  CUE_SINIT_FAILED      = 22,  /**< Suite initialization failed. */
  CUE_SCLEAN_FAILED     = 23,  /**< Suite cleanup failed. */
  CUE_DUP_SUITE         = 24,  /**< Duplicate suite name not allowed. */

  /* Test Case Level Errors */
  CUE_NOTEST            = 30,  /**< A required CU_pTest pointer was NULL. */
  CUE_NO_TESTNAME       = 31,  /**< Required CU_Test name not provided. */
  CUE_DUP_TEST          = 32,  /**< Duplicate test case name not allowed. */
  CUE_TEST_NOT_IN_SUITE = 33,  /**< Test not registered in specified suite. */

  /* File handling errors */
  CUE_FOPEN_FAILED      = 40,  /**< An error occurred opening a file. */
  CUE_FCLOSE_FAILED     = 41,  /**< An error occurred closing a file. */
  CUE_BAD_FILENAME      = 42,  /**< A bad filename was requested (NULL, empty, nonexistent, etc.). */
  CUE_WRITE_ERROR       = 43   /**< An error occurred during a write to a file. */
} CU_ErrorCode;

/*------------------------------------------------------------------------*/
/** CUnit error action codes.
 *  These are used to set the action desired when an error
 *  condition is detected in the CUnit framework.
 *  @see CU_set_error_action()
 *  @see CU_get_error_action()
 */
typedef enum CU_ErrorAction {
  CUEA_IGNORE,    /**< Runs should be continued when an error condition occurs (if possible). */
  CUEA_FAIL,      /**< Runs should be stopped when an error condition occurs. */
  CUEA_ABORT      /**< The application should exit() when an error conditions occurs. */
} CU_ErrorAction;

/* Error handling & reporting functions. */

#include "CUnit.h"

#ifdef __cplusplus
extern "C" {
#endif

CU_EXPORT CU_ErrorCode   CU_get_error(void);
CU_EXPORT const char*    CU_get_error_msg(void);
CU_EXPORT void           CU_set_error_action(CU_ErrorAction action);
CU_EXPORT CU_ErrorAction CU_get_error_action(void);

#ifdef CUNIT_BUILD_TESTS
void test_cunit_CUError(void);
#endif

/* Internal function - users should not generally call this function */
void  CU_set_error(CU_ErrorCode error);

#ifdef __cplusplus
}
#endif

#ifdef USE_DEPRECATED_CUNIT_NAMES
/** Deprecated (version 1). @deprecated Use CU_get_error_msg(). */
#define get_error() CU_get_error_msg()
#endif  /* USE_DEPRECATED_CUNIT_NAMES */

#endif  /*  CUNIT_CUERROR_H_SEEN  */
/** @} */
