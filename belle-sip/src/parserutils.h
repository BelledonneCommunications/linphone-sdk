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
#ifndef belle_sip_parserutils_h
#define belle_sip_parserutils_h

#include "port.h"

#define IS_TOKEN(token) \
		(INPUT->toStringTT(INPUT,LT(1),LT(strlen(#token)))->chars ?\
		strcasecmp(#token,(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(strlen(#token)))->chars)) == 0:0)


#define IS_HEADER_NAMED(name,compressed_name) (IS_TOKEN(compressed_name) || IS_TOKEN(name))

#define STRCASECMP_HEADER_NAMED(name,compressed_name,value) \
		(strcasecmp(compressed_name,(const char*)value) == 0 || strcasecmp(name,(const char*)value) == 0 )

#define ANTLR3_LOG_EXCEPTION() belle_sip_message("[\%s] reason [\%s] at line[\%u] position[\%d]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message,EXCEPTION->line,EXCEPTION->charPositionInLine);


BELLESIP_INTERNAL_EXPORT belle_sip_header_t* belle_sip_header_get_next(const belle_sip_header_t* headers);
BELLESIP_INTERNAL_EXPORT void belle_sip_header_set_next(belle_sip_header_t* header,belle_sip_header_t* next);

BELLESIP_INTERNAL_EXPORT char* belle_sip_to_unescaped_string(const char* buff);
belle_sip_param_pair_t* belle_sip_param_pair_new(const char* name,const char* value);
char* _belle_sip_str_dup_and_unquote_string(const char* quoted_string);

/**
 * quoted-string  =  SWS DQUOTE *(qdtext / quoted-pair ) DQUOTE
      qdtext         =  LWS / %x21 / %x23-5B / %x5D-7E
                        / UTF8-NONASCII
 quoted-pair  =  "\" (%x00-09 / %x0B-0C
                / %x0E-7F)

remove any \
 * */
BELLESIP_INTERNAL_EXPORT char* belle_sip_string_to_backslash_less_unescaped_string(const char* buff);
BELLESIP_INTERNAL_EXPORT char* belle_sip_display_name_to_backslashed_escaped_string(const char* buff);

#endif
