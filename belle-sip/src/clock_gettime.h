/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef CLOCK_GETTIME_H_
#define CLOCK_GETTIME_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBRT /*configure defines HAVE_LIBRT with AC_CHECK_LIB*/
#	ifndef HAVE_CLOCK_GETTIME
#	define HAVE_CLOCK_GETTIME 1
#endif
#endif

#ifndef HAVE_CLOCK_GETTIME

#include <sys/time.h>
#include <sys/resource.h>
#include <mach/mach.h>
#include <mach/clock.h>
#include <mach/mach_time.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>


typedef enum {
	CLOCK_REALTIME,
	CLOCK_MONOTONIC,
	CLOCK_PROCESS_CPUTIME_ID,
	CLOCK_THREAD_CPUTIME_ID
} clockid_t;


int clock_gettime(clockid_t clk_id, struct timespec *tp) ;
#endif

#endif /* CLOCK_GETTIME_H_ */
