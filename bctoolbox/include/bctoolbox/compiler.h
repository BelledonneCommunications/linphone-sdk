/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
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

#ifndef COMPILER_H
#define COMPILER_H

#ifdef __has_feature
	#if __has_feature(address_sanitizer)
		#define BCTBX_ASAN_ENABLED
	#endif // if __has_feature(address_sanitizer)
#elif defined(__SANITIZE_ADDRESS__)
	#define BCTBX_ASAN_ENABLED
#endif // ifdef __has_feature

#ifdef BCTBX_ASAN_ENABLED
	#define BCTBX_DISABLE_ASAN __attribute__((no_sanitize_address))
#else
	#define BCTBX_DISABLE_ASAN
#endif // ifdef BCTBX_ASAN_ENABLED

#ifdef __has_attribute
	#if __has_attribute(no_sanitize)
		#define BCTBX_DISABLE_UBSAN __attribute__((no_sanitize("undefined")))
	#else
		#define BCTBX_DISABLE_UBSAN
	#endif // __has_attribute(no_sanitize)
#elif defined(__GNUC__) && !defined(__MINGW32__) && GCC_VERSION >= 40900
	#define BCTBX_DISABLE_UBSAN __attribute__((no_sanitize_undefined))
#else
	#define BCTBX_DISABLE_UBSAN
#endif // ifdef __has_attribute

#endif // ifdef COMPILER_H
