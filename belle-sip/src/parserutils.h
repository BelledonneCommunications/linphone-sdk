/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef belle_sip_parserutils_h
#define belle_sip_parserutils_h

#include "port.h"

static BELLESIP_INLINE int belle_sip_strcasecmp(const char *a, const char *b) {
	if (!a || !b) return 0;
	return strcasecmp(a, b) == 0;
}

#define IS_TOKEN(token)                                                                                                \
	belle_sip_strcasecmp(#token, (const char *)INPUT->toStringTT(INPUT, LT(1), LT(strlen(#token)))->chars)

#define IS_HEADER_NAMED(name, compressed_name) (IS_TOKEN(compressed_name) || IS_TOKEN(name))

#define STRCASECMP_HEADER_NAMED(name, compressed_name, value)                                                          \
	(strcasecmp(compressed_name, (const char *)value) == 0 || strcasecmp(name, (const char *)value) == 0)

#ifdef __cplusplus
extern "C" {
#endif

char *belle_sip_trim_whitespaces(char *str);

BELLESIP_EXPORT void belle_sip_header_set_next(belle_sip_header_t *header, belle_sip_header_t *next);

belle_sip_param_pair_t *belle_sip_param_pair_new(const char *name, const char *value);
char *_belle_sip_str_dup_and_unquote_string(const char *quoted_string);

/**
 * quoted-string  =  SWS DQUOTE *(qdtext / quoted-pair ) DQUOTE
      qdtext         =  LWS / %x21 / %x23-5B / %x5D-7E
                        / UTF8-NONASCII
 quoted-pair  =  "\" (%x00-09 / %x0B-0C
                / %x0E-7F)

remove any \
 * */
BELLESIP_EXPORT char *belle_sip_string_to_backslash_less_unescaped_string(const char *buff);
BELLESIP_EXPORT char *belle_sip_display_name_to_backslashed_escaped_string(const char *buff);

#ifdef __cplusplus
}
#endif

#endif
