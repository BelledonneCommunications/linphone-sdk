/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef BELLE_SIP_UTILS_H
#define BELLE_SIP_UTILS_H

#include <stdarg.h>
#include <stdio.h>

/***************/
/* logging api */
/***************/

typedef enum {
        BELLE_SIP_DEBUG=1,
        BELLE_SIP_MESSAGE=1<<1,
        BELLE_SIP_WARNING=1<<2,
        BELLE_SIP_ERROR=1<<3,
        BELLE_SIP_FATAL=1<<4,
        BELLE_SIP_LOGLEV_END=1<<5
} belle_sip_log_level;

#define BELLE_SIP_LOG_ALL	(0xffff)

typedef void (*belle_sip_log_function_t)(belle_sip_log_level lev, const char *fmt, va_list args);

BELLE_SIP_BEGIN_DECLS

void belle_sip_set_log_file(FILE *file);
void belle_sip_set_log_handler(belle_sip_log_function_t func);


void belle_sip_set_log_level_mask(int levelmask);

BELLE_SIP_END_DECLS

#endif

