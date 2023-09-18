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

#define _CRT_RAND_S
#include "bctoolbox/crypto.h"
#include "bctoolbox/parser.h"
#include "belle_sip_internal.h"
#include <stddef.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <dirent.h> /* available on POSIX system only */
#include <unistd.h>
#else
#include <direct.h>
#endif

belle_sip_error_code belle_sip_snprintf(char *buff, size_t buff_size, size_t *offset, const char *fmt, ...) {
	belle_sip_error_code ret;
	va_list args;
	va_start(args, fmt);
	ret = belle_sip_snprintf_valist(buff, buff_size, offset, fmt, args);
	va_end(args);

	return ret;
}

belle_sip_error_code
belle_sip_snprintf_valist(char *buff, size_t buff_size, size_t *offset, const char *fmt, va_list args) {
	int ret;
	belle_sip_error_code error = BELLE_SIP_OK;
	ret = vsnprintf(buff + *offset, buff_size - *offset, fmt, args);
	if ((ret < 0) || (ret >= (int)(buff_size - *offset))) {
		error = BELLE_SIP_BUFFER_OVERFLOW;
		*offset = buff_size;
	} else {
		*offset += ret;
	}
	return error;
}

#if defined(_WIN32) || defined(_WIN32_WCE)
#define ENDLINE "\r\n"
#else
#define ENDLINE "\n"
#endif

uint64_t belle_sip_time_ms(void) {
	return bctbx_get_cur_time_ms();
}
/**
 * parser parameter pair
 */

belle_sip_param_pair_t *belle_sip_param_pair_new(const char *name, const char *value) {
	belle_sip_param_pair_t *lPair = (belle_sip_param_pair_t *)belle_sip_new0(belle_sip_param_pair_t);
	lPair->name = name ? belle_sip_strdup(name) : NULL;
	lPair->value = value ? belle_sip_strdup(value) : NULL;
	return lPair;
}

void belle_sip_param_pair_destroy(belle_sip_param_pair_t *pair) {
	if (pair->name) belle_sip_free(pair->name);
	if (pair->value) belle_sip_free(pair->value);
	belle_sip_free(pair);
}

int belle_sip_param_pair_comp_func(const belle_sip_param_pair_t *a, const char *b) {
	return strcmp(a->name, b);
}
int belle_sip_param_pair_case_comp_func(const belle_sip_param_pair_t *a, const char *b) {
	return strcasecmp(a->name, b);
}

char *_belle_sip_str_dup_and_unquote_string(const char *quoted_string) {
	size_t value_size = strlen(quoted_string);
	char *unquoted_string = belle_sip_malloc0(value_size - 2 + 1);
	strncpy(unquoted_string, quoted_string + 1, value_size - 2);
	return unquoted_string;
}

char *belle_sip_unquote_strdup(const char *str) {
	const char *p;
	if (str == NULL) return NULL;

	for (p = str; *p != '\0'; ++p) {
		switch (*p) {
			case ' ':
			case '\t':
				break;
			case '"':
				return _belle_sip_str_dup_and_unquote_string(p);
			default:
				return belle_sip_strdup(str);
				break;
		}
	}
	return belle_sip_strdup(str);
}

void belle_sip_set_socket_api(bctbx_vsocket_api_t *my_api) {

	bctbx_vsocket_api_set_default(my_api);
}

static const char *symbols = "aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789-~";

/**
 * Write a random text token of supplied size.
 **/
char *belle_sip_random_token(char *ret, size_t size) {
	return belle_sip_random_token_with_charset(ret, size, symbols, 64);
}

char *belle_sip_random_token_with_charset(char *ret, size_t size, const char *charset, size_t charset_length) {
	unsigned int i;
	belle_sip_random_bytes((unsigned char *)ret, size - 1);
	for (i = 0; i < size - 1; ++i) {
		ret[i] = charset[ret[i] % charset_length];
	}
	ret[i] = 0;
	return ret;
}

typedef struct bits_reader {
	const uint8_t *buffer;
	size_t buf_size;
	int bit_index;
} bits_reader_t;

static void bits_reader_init(bits_reader_t *reader, const uint8_t *buffer, size_t bufsize) {
	reader->buffer = buffer;
	reader->buf_size = bufsize;
	reader->bit_index = 0;
}

static int bits_reader_read(bits_reader_t *reader, int count, unsigned int *ret) {
	unsigned int tmp;
	size_t byte_index = reader->bit_index / 8;
	size_t bit_index = reader->bit_index % 8;
	int shift = 32 - (int)bit_index - count;

	if (count >= 24) {
		belle_sip_error("This bit reader cannot read more than 24 bits at once.");
		return -1;
	}

	if (byte_index < reader->buf_size) tmp = ((unsigned int)reader->buffer[byte_index++]) << 24;
	else {
		belle_sip_error("Bit reader goes end of stream.");
		return -1;
	}
	if (byte_index < reader->buf_size) tmp |= ((unsigned int)reader->buffer[byte_index++]) << 16;
	if (byte_index < reader->buf_size) tmp |= ((unsigned int)reader->buffer[byte_index++]) << 8;
	if (byte_index < reader->buf_size) tmp |= ((unsigned int)reader->buffer[byte_index++]);

	tmp = tmp >> shift;
	tmp = tmp & ((1 << count) - 1);
	reader->bit_index += count;
	*ret = tmp;
	return 0;
}

char *belle_sip_octets_to_text(const uint8_t *hash, size_t hash_len, char *ret, size_t size) {
	int i;
	bits_reader_t bitctx;

	bits_reader_init(&bitctx, hash, hash_len);

	for (i = 0; i < (int)size - 1; ++i) {
		unsigned int val = 0;
		if (bits_reader_read(&bitctx, 6, &val) == 0) {
			ret[i] = symbols[val];
		} else break;
	}
	ret[i] = 0;
	return ret;
}

void belle_sip_util_copy_headers(belle_sip_message_t *orig,
                                 belle_sip_message_t *dest,
                                 const char *header,
                                 int multiple) {
	const belle_sip_list_t *elem;
	elem = belle_sip_message_get_headers(orig, header);
	for (; elem != NULL; elem = elem->next) {
		belle_sip_header_t *ref_header = (belle_sip_header_t *)elem->data;
		if (ref_header) {
			ref_header = (belle_sip_header_t *)belle_sip_object_clone((belle_sip_object_t *)ref_header);
			if (!multiple) {
				belle_sip_message_set_header(dest, ref_header);
				break;
			} else belle_sip_message_add_header(dest, ref_header);
		}
	}
}

char *belle_sip_to_unescaped_string(const char *buff) {
	return bctbx_unescaped_string(buff);
}

size_t belle_sip_get_char(const char *a, char *b) {
	return bctbx_get_char(a, b);
}
/*
static void print_noescapes_map(char noescapes[BELLE_SIP_NO_ESCAPES_SIZE], const char *name) {
    unsigned int i;
    printf("Noescapes %s :", name);
    for (i=' '; i <= '~'; ++i) {
        if (noescapes[i] == 1) printf ("%c", i);
        //if (noescapes[i] == 1) printf ("%c %d - %d\n", i, (char)i, noescapes[i]);
    }
    printf ("init: %d\n", noescapes[BELLE_SIP_NO_ESCAPES_SIZE-1]);
}
*/

static const bctbx_noescape_rules_t *get_sip_uri_username_noescapes(void) {
	static bctbx_noescape_rules_t noescapes = {0};
	if (noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] == 0) {
		// concurrent initialization should not be an issue
		/*user             =  1*( unreserved / escaped / user-unreserved )
		 unreserved  =  alphanum / mark
		 mark        =  "-" / "_" / "." / "!" / "~" / "*" / "'"
		 / "(" / ")"
		user-unreserved  =  "&" / "=" / "+" / "$" / "," / ";" / "?" / "/"
		*/
		bctbx_noescape_rules_add_alfanums(noescapes);
		/*mark*/
		bctbx_noescape_rules_add_list(noescapes, "-_.!~*'()");
		/*user-unreserved*/
		bctbx_noescape_rules_add_list(noescapes, "&=+$,;?/");

		noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] = 1; // initialized
		                                                //		print_noescapes_map(noescapes, "uri_username");
	}
	return (const bctbx_noescape_rules_t *)&noescapes; /*gcc asks for a cast, clang not*/
}
/*
 *
 * password         =  *( unreserved / escaped /
                    "&" / "=" / "+" / "$" / "," )
 * */
static const bctbx_noescape_rules_t *get_sip_uri_userpasswd_noescapes(void) {
	static bctbx_noescape_rules_t noescapes = {0};
	if (noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] == 0) {
		// unreserved
		bctbx_noescape_rules_add_alfanums(noescapes);
		bctbx_noescape_rules_add_list(noescapes, "-_.!~*'()");
		bctbx_noescape_rules_add_list(noescapes, "&=+$,");

		noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] = 1; // initialized
	}
	return (const bctbx_noescape_rules_t *)&noescapes; /*gcc asks for a cast, clang not*/
}

static const bctbx_noescape_rules_t *get_sip_uri_parameter_noescapes(void) {
	static bctbx_noescape_rules_t noescapes = {0};
	if (noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] == 0) {
		/*
		 other-param       =  pname [ "=" pvalue ]
		 pname             =  1*paramchar
		 pvalue            =  1*paramchar
		 paramchar         =  param-unreserved / unreserved / escaped
		 param-unreserved  =  "[" / "]" / "/" / ":" / "&" / "+" / "$"
		 unreserved  =  alphanum / mark
		 mark        =  "-" / "_" / "." / "!" / "~" / "*" / "'"
		 / "(" / ")"
		 escaped     =  "%" HEXDIG HEXDIG
		 token       =  1*(alphanum / "-" / "." / "!" / "%" / "*"
		 / "_" / "+" / "`" / "'" / "~" )
		 */
		// param-unreserved  =

		bctbx_noescape_rules_add_list(noescapes, "[]/:&+$");

		// token
		bctbx_noescape_rules_add_alfanums(noescapes);
		bctbx_noescape_rules_add_list(noescapes, "-.!%*_+`'~");

		// unreserved
		bctbx_noescape_rules_add_list(noescapes, "-_.!~*'()");

		noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] = 1; // initialized
		                                                //		print_noescapes_map(noescapes, "uri_parameter");
	}
	return (const bctbx_noescape_rules_t *)&noescapes; /*gcc asks for a cast, clang not*/
}
static const bctbx_noescape_rules_t *get_sip_uri_header_noescapes(void) {
	static bctbx_noescape_rules_t noescapes = {0};
	if (noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] == 0) {
		/*
		 unreserved  =  alphanum / mark
		 mark        =  "-" / "_" / "." / "!" / "~" / "*" / "'"
		 / "(" / ")"
		 escaped     =  "%" HEXDIG HEXDIG

		 //....
		header          =  hname "=" hvalue
		hname           =  1*( hnv-unreserved / unreserved / escaped )
		hvalue          =  *( hnv-unreserved / unreserved / escaped )
		hnv-unreserved  =  "[" / "]" / "/" / "?" / ":" / "+" / "$"

		 */

		// unreserved
		// alphanum
		bctbx_noescape_rules_add_alfanums(noescapes);
		// mark
		bctbx_noescape_rules_add_list(noescapes, "-_.!~*'()");

		bctbx_noescape_rules_add_list(noescapes, "[]/?:+$");
		// hnv-unreserved
		noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] = 1; // initialized
		                                                //		print_noescapes_map(noescapes, "uri_parameter");
	}
	return (const bctbx_noescape_rules_t *)&noescapes; /*gcc asks for a cast, clang not*/
}

char *belle_sip_uri_to_escaped_username(const char *buff) {
	return bctbx_escape(buff, *get_sip_uri_username_noescapes());
}
char *belle_sip_username_unescape_unnecessary_characters(const char *buff) {
	return bctbx_unescaped_string_only_chars_in_rules(buff, *get_sip_uri_username_noescapes());
}
char *belle_sip_uri_to_escaped_userpasswd(const char *buff) {
	return bctbx_escape(buff, *get_sip_uri_userpasswd_noescapes());
}
char *belle_sip_uri_to_escaped_parameter(const char *buff) {
	return bctbx_escape(buff, *get_sip_uri_parameter_noescapes());
}
char *belle_sip_uri_to_escaped_header(const char *buff) {
	return bctbx_escape(buff, *get_sip_uri_header_noescapes());
}

/*uri (I.E RFC 2396)*/
static const bctbx_noescape_rules_t *get_generic_uri_query_noescapes(void) {
	static bctbx_noescape_rules_t noescapes = {0};
	if (noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] == 0) {
		/*
		uric          = reserved | unreserved | escaped
		reserved      = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |
		                "$" | ","
		unreserved    = alphanum | mark
		mark          = "-" | "_" | "." | "!" | "~" | "*" | "'" |
		                      "(" | ")"

		3.4. Query Component
		  query         = *uric
	   Within a query component, the characters ";", "/", "?", ":", "@",
	   "&", "=", "+", ",", and "$" are reserved.

		*/
		/*unreserved*/
		bctbx_noescape_rules_add_alfanums(noescapes);
		/*mark*/
		bctbx_noescape_rules_add_list(noescapes, "-_.!~*'()");
		bctbx_noescape_rules_add_list(noescapes, "=&"); // otherwise how to pass parameters?
		noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] = 1; // initialized
	}
	return (const bctbx_noescape_rules_t *)&noescapes; /*gcc asks for a cast, clang not*/
}

static const bctbx_noescape_rules_t *get_generic_uri_path_noescapes(void) {
	static bctbx_noescape_rules_t noescapes = {0};
	if (noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] == 0) {
		/*
		3.3. Path Component

   The path component contains data, specific to the authority (or the
   scheme if there is no authority component), identifying the resource
   within the scope of that scheme and authority.

	  path          = [ abs_path | opaque_part ]

	  path_segments = segment *( "/" segment )
	  segment       = *pchar *( ";" param )
	  param         = *pchar

	  pchar         = unreserved | escaped |
		              ":" | "@" | "&" | "=" | "+" | "$" | ","

   The path may consist of a sequence of path segments separated by a
   single slash "/" character.  Within a path segment, the characters
   "/", ";", "=", and "?" are reserved.  Each path segment may include a
   sequence of parameters, indicated by the semicolon ";" character.
   The parameters are not significant to the parsing of relative
   references.

		*/
		/*unreserved*/
		bctbx_noescape_rules_add_alfanums(noescapes);
		/*mark*/
		bctbx_noescape_rules_add_list(noescapes, "-_.!~*'()");
		/*pchar*/
		bctbx_noescape_rules_add_list(noescapes, ":@&=+$,");
		/*;*/
		bctbx_noescape_rules_add_list(noescapes, ";");
		bctbx_noescape_rules_add_list(noescapes, "/");

		noescapes[BCTBX_NOESCAPE_RULES_USER_INDEX] = 1; // initialized
	}
	return (const bctbx_noescape_rules_t *)&noescapes; /*gcc asks for a cast, clang not*/
}

char *belle_generic_uri_to_escaped_query(const char *buff) {
	return bctbx_escape(buff, *get_generic_uri_query_noescapes());
}
char *belle_generic_uri_to_escaped_path(const char *buff) {
	return bctbx_escape(buff, *get_generic_uri_path_noescapes());
}

char *belle_sip_string_to_backslash_less_unescaped_string(const char *buff) {
	size_t buff_len = strlen(buff);
	char *output_buff = belle_sip_malloc(buff_len + 1);
	unsigned int i;
	unsigned int out_buff_index = 0;

	for (i = 0; buff[i] != '\0'; i++) {
		if (buff[i] == '\\' && i + 1 < buff_len) { /*make sure escaped caracter exist*/
			i++;                                   /*skip \*/
		}
		/*make sure to only remove one \ in case of \\*/
		output_buff[out_buff_index++] = buff[i];
	}
	output_buff[out_buff_index] = '\0';
	return output_buff;
}
char *belle_sip_display_name_to_backslashed_escaped_string(const char *buff) {
	char output_buff[BELLE_SIP_MAX_TO_STRING_SIZE];
	unsigned int i;
	unsigned int out_buff_index = 0;
	for (i = 0; buff[i] != '\0' && out_buff_index < sizeof(output_buff) - 2; i++) {
		/*-3 to make sure last param can be stored in escaped form*/
		const char c = buff[i];
		if (c == '\"' || c == '\\') {
			output_buff[out_buff_index++] = '\\'; /*insert escape character*/
		}
		output_buff[out_buff_index++] = c;
	}
	output_buff[out_buff_index] = '\0';
	return belle_sip_strdup(output_buff);
}
