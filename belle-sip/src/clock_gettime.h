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
#ifdef __APPLE__
typedef enum {
	BC_CLOCK_REALTIME,
	BC_CLOCK_MONOTONIC,
	BC_CLOCK_PROCESS_CPUTIME_ID,
	BC_CLOCK_THREAD_CPUTIME_ID
} bc_clockid_t;


int bc_clock_gettime(bc_clockid_t clk_id, struct timespec *tp) ;
#endif

#endif /* CLOCK_GETTIME_H_ */
