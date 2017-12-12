/*
	bctoolbox
	Copyright (C) 2016  Belledonne Communications SARL.
 
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as
	published by the Free Software Foundation, either version 3 of the
	License, or (at your option) any later version.
 
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.
 
	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BCTBX_CHARCONV_H
#define BCTBX_CHARCONV_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert the given string from system locale to UTF8.
 *
 * @param[in]		str			string to convert
 *
 * @return a pointer to a null-terminated string containing the converted string. This buffer must then be freed
 * by caller. NULL on failure.
 */
BCTBX_PUBLIC char *bctbx_locale_to_utf8(const char *str);

/**
 * @brief Convert the given string from UTF8 to system locale.
 *
 * @param[in]		str			string to convert
 *
 * @return a pointer to a null-terminated string containing the converted string. This buffer must then be freed
 * by caller. NULL on failure.
 */
BCTBX_PUBLIC char *bctbx_utf8_to_locale(const char *str);

/**
 * @brief Convert the given string.
 *
 * @param[in]		str			string to convert
 * @param[in]		from		charset of the string
 * @param[in]		to			charset to convert
 *
 * @return a pointer to a null-terminated string containing the converted string. This buffer must then be freed
 * by caller. NULL on failure.
 *
 * @note If from or to is equal to "locale" then it will use the system's locale
 * @note If from and to are equals then it returns a copy of str
 */
BCTBX_PUBLIC char *bctbx_convert_from_to(const char *str, const char *from, const char *to);

#ifdef __cplusplus
}
#endif

#endif /* BCTBX_CHARCONV_H */

