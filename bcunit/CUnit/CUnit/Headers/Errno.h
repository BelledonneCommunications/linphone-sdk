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
 */

#ifndef _Errno_h_
#define _Errno_h_

#include <errno.h>

#define CUE_SUCCESS		0
#define CUE_NOMEMORY		1

/* Test Registry Level Errors */
#define CUE_NOREGISTRY		10
#define CUE_REGISTRY_EXISTS	11

/* Test Group Level Errors */
#define CUE_NOGROUP		20
#define CUE_NO_GROUPNAME	21
#define CUE_GRPINIT_FAILED	22
#define CUE_GRPCLEAN_FAILED	23

/* Test Case Level Errors */
#define CUE_NOTEST		30
#define CUE_NO_TESTNAME		31

#endif  /*  _Errno_h_  */
