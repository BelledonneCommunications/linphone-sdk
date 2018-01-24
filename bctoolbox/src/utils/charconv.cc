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

#ifdef __APPLE__
   #include "TargetConditionals.h"
#endif

#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"
#include "bctoolbox/charconv.h"

#ifdef _WIN32
#include <algorithm>
#include <unordered_map>

static std::unordered_map <std::string, UINT> windows_charset {
	{ "LOCALE", CP_ACP },
	{ "IBM037", 037 },
	{ "IBM437", 437 },
	{ "IBM500", 500 },
	{ "ASMO-708", 708 },
	{ "IBM775", 775 },
	{ "IBM850", 850 },
	{ "IBM852", 852 },
	{ "IBM855", 855 },
	{ "IBM857", 857 },
	{ "IBM860", 860 },
	{ "IBM861", 861 },
	{ "IBM863", 863 },
	{ "IBM864", 864 },
	{ "IBM865", 865 },
	{ "CP866", 866 },
	{ "IBM869", 869 },
	{ "IBM870", 870 },
	{ "WINDOWS-874", 874 },
	{ "CP875", 875 },
	{ "SHIFT_JIS", 932 },
	{ "GB2312", 936 },
	{ "BIG5", 950 },
	{ "IBM1026", 1026 },
	{ "UTF-16", 1200 },
	{ "WINDOWS-1250", 1250 },
	{ "WINDOWS-1251", 1251 },
	{ "WINDOWS-1252", 1252 },
	{ "WINDOWS-1253", 1253 },
	{ "WINDOWS-1254", 1254 },
	{ "WINDOWS-1255", 1255 },
	{ "WINDOWS-1256", 1256 },
	{ "WINDOWS-1257", 1257 },
	{ "WINDOWS-1258", 1258 },
	{ "JOHAB", 1361 },
	{ "MACINTOSH", 10000 },
	{ "UTF-32", 12000 },
	{ "UTF-32BE", 12001 },
	{ "US-ASCII", 20127 },
	{ "IBM273", 20273 },
	{ "IBM277", 20277 },
	{ "IBM278", 20278 },
	{ "IBM280", 20280 },
	{ "IBM284", 20284 },
	{ "IBM285", 20285 },
	{ "IBM290", 20290 },
	{ "IBM297", 20297 },
	{ "IBM420", 20420 },
	{ "IBM423", 20423 },
	{ "IBM424", 20424 },
	{ "KOI8-R", 20866 },
	{ "IBM871", 20871 },
	{ "IBM880", 20880 },
	{ "IBM905", 20905 },
	{ "EUC-JP", 20932 },
	{ "CP1025", 21025 },
	{ "KOI8-U", 21866 },
	{ "ISO-8859-1", 28591 },
	{ "ISO-8859-2", 28592 },
	{ "ISO-8859-3", 28593 },
	{ "ISO-8859-4", 28594 },
	{ "ISO-8859-5", 28595 },
	{ "ISO-8859-6", 28596 },
	{ "ISO-8859-7", 28597 },
	{ "ISO-8859-8", 28598 },
	{ "ISO-8859-9", 28599 },
	{ "ISO-8859-13", 28603 },
	{ "ISO-8859-15", 28605 },
	{ "ISO-2022-JP", 50222 },
	{ "CSISO2022JP", 50221 },
	{ "ISO-2022-KR", 50225 },
	{ "EUC-JP", 51932 },
	{ "EUC-CN", 51936 },
	{ "EUC-KR", 51949 },
	{ "GB18030", 54936 },
	{ "UTF-7", 65000 },
	{ "UTF-8", 65001 }
};

std::string string_to_upper (const std::string &str) {
	std::string result(str.size(), ' ');
	std::transform(str.cbegin(), str.cend(), result.begin(), ::toupper);
	return result;
}
#endif

extern "C" char *bctbx_locale_to_utf8(const char *str) {
#if defined(__ANDROID__) || TARGET_OS_IPHONE
	// TODO remove this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
#else
	return bctbx_convert_from_to(str, "locale", "UTF-8");
#endif
}

extern "C" char *bctbx_utf8_to_locale(const char *str) {
#if defined(__ANDROID__) || TARGET_OS_IPHONE
	// TODO remove this part when the NDK will contain a usable iconv
	return bctbx_strdup(str);
#else
	return bctbx_convert_from_to(str, "UTF-8", "locale");
#endif
}

extern "C" char *bctbx_convert_from_to(const char *str, const char *from, const char *to) {
	if (!from || !to)
		return NULL;

	if (strcasecmp(from, to) == 0)
		return bctbx_strdup(str);

#if defined(_WIN32)
	char* converted_str;
	int n_char, nb_byte;
	LPWSTR wide_str;
	UINT r_from, r_to;

	try {
		r_from = windows_charset.at(string_to_upper(std::string(from)));
		r_to = windows_charset.at(string_to_upper(std::string(to)));
	}
	catch (const std::out_of_range&) {
		bctbx_error("Error while converting a string from '%s' to '%s': unknown charset", from, to);
		return FALSE;
	}

	n_char = MultiByteToWideChar(r_from, 0, str, -1, NULL, 0);
	if (n_char == 0) return NULL;
	wide_str = (LPWSTR) bctbx_malloc(n_char*sizeof(wide_str[0]));
	if (wide_str == NULL) return NULL;
	n_char = MultiByteToWideChar(r_from, 0, str, -1, wide_str, n_char);
	if (n_char == 0) {
		bctbx_free(wide_str);
		wide_str = 0;
	}

	nb_byte = WideCharToMultiByte(r_to, 0, wide_str, -1, 0, 0, 0, 0);
	if (nb_byte == 0) return NULL;
	converted_str = (char *) bctbx_malloc(nb_byte);
	if (converted_str == NULL) return NULL;
	nb_byte = WideCharToMultiByte(r_to, 0, wide_str, -1, converted_str, nb_byte, 0, 0);
	if (nb_byte == 0) {
		bctbx_free(converted_str);
		converted_str = 0;
	}
	bctbx_free(wide_str);
	return converted_str;
#elif defined(__ANDROID__)
	// TODO remove this part when the NDK will contain a usable iconv
	bctbx_error("Unable to convert a string in Android: iconv is not available");
	return NULL;
#else
	char *in_buf = (char *) str;
	char *out_buf, *ptr;
	size_t in_left = strlen(str) + 1;
	size_t out_left = in_left + in_left/10; // leave a marge of 10%
	iconv_t cd;

	setlocale(LC_CTYPE, ""); // Retrieve environment locale before calling nl_langinfo

	const char* r_from = strcasecmp("locale", from) == 0 ? nl_langinfo(CODESET) : from;
	const char* r_to = strcasecmp("locale", to) == 0 ? nl_langinfo(CODESET) : to;

	if (strcasecmp(r_from, r_to) == 0) {
		return bctbx_strdup(str);
	}

	cd = iconv_open(r_to, r_from);

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

		if (ret == (size_t)-1 && errno != E2BIG) {
			bctbx_error("Error while converting a string from '%s' to '%s': %s", from, to, strerror(errno));
			bctbx_free(ptr);
			return NULL;
		}
	} else {
		bctbx_error("Unable to open iconv content descriptor from '%s' to '%s': %s", from, to, strerror(errno));
		return NULL;
	}
	return ptr;
#endif
}
