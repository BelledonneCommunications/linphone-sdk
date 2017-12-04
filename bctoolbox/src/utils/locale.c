/*
	bctoolbox
	Copyright (C) 2016  Belledonne Communications SARL

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

#if !defined(_WIN32) && !defined(__QNXNTO__) && !defined(__ANDROID__)
#	include <langinfo.h>
#	include <locale.h>
#	include <iconv.h>
#	include <string.h>
#	include <errno.h>
#endif

#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"

char *bctbx_convert_utf8_locale(const char *str, bool_t locale_to_utf8) {
#if defined(_WIN32)
	char* convertedStr;
	int nChar, nb_byte;
	LPWSTR wideStr;
	UINT from = locale_to_utf8 ? CP_ACP : CP_UTF8;
	UINT to = locale_to_utf8 ? CP_UTF8 : CP_ACP;
	
	nChar = MultiByteToWideChar(from, 0, str, -1, NULL, 0);
	if (nChar == 0) return NULL;
	wideStr = (LPWSTR) bctbx_malloc(nChar*sizeof(wideStr[0]));
	if (wideStr == NULL) return NULL;
	nChar = MultiByteToWideChar(from, 0, str, -1, wideStr, nChar);
	if (nChar == 0) {
		bctbx_free(wideStr);
		wideStr = 0;
	}
	
	nb_byte = WideCharToMultiByte(to, 0, wideStr, -1, 0, 0, 0, 0);
	if (nb_byte == 0) return NULL;
	convertedStr = (char *) bctbx_malloc(nb_byte);
	if (convertedStr == NULL) return NULL;
	nb_byte = WideCharToMultiByte(to, 0, wideStr, -1, convertedStr, nb_byte, 0, 0);
	if (nb_byte == 0) {
		bctbx_free(convertedStr);
		convertedStr = 0;
	}
	bctbx_free(wideStr);
	return convertedStr;
#elif defined(__QNXNTO__) || defined(__ANDROID__)
	return bctbx_strdup(str);
#else
	if (strcasecmp("UTF-8", nl_langinfo(CODESET)) == 0) {
		return bctbx_strdup(str);
	} else {
		char *in_buf = (char *) str;
		char *out_buf, *ptr;
		size_t in_left = strlen(str) + 1;
		size_t out_left = in_left + in_left/10; // leave a marge of 10%
		iconv_t cd;
		
		if (locale_to_utf8)
			cd = iconv_open("UTF-8", nl_langinfo(CODESET));
		else
			cd = iconv_open(nl_langinfo(CODESET), "UTF-8");
		
		if (cd != (iconv_t)-1) {
			size_t ret;
			size_t out_len = out_left;
			
			out_buf = (char *) bctbx_malloc(out_left);
			ptr = out_buf; // Keep a pointer to the beginning of this buffer to be able to realloc
			
			ret = iconv(cd, &in_buf, &in_left, &out_buf, &out_left);
			while (ret == (size_t)-1 && errno == E2BIG) {
				ptr = (char *) bctbx_realloc(ptr, out_len*2);
				out_left = out_len;
				out_buf = ptr + out_left;
				out_len *= 2; 
				
				ret = iconv(cd, &in_buf, &in_left, &out_buf, &out_left);
			}
			
			iconv_close(cd);
		} else {
			bctbx_error("Unable to open iconv content descriptor: %s", strerror(errno));
			return NULL;
		}
		return ptr;
	}
#endif
}

char *bctbx_locale_to_utf8(const char *str) {
	return bctbx_convert_utf8_locale(str, TRUE);
}

char *bctbx_utf8_to_locale(const char *str) {
	return bctbx_convert_utf8_locale(str, FALSE);
}
