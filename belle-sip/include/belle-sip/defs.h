/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010-2013  Belledonne Communications SARL

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

#ifndef BELLE_SIP_DEFS_H
#define BELLE_SIP_DEFS_H

#ifdef __cplusplus
#define BELLE_SIP_BEGIN_DECLS		extern "C"{
#define BELLE_SIP_END_DECLS		}
#else
#define BELLE_SIP_BEGIN_DECLS
#define BELLE_SIP_END_DECLS
#endif

#ifdef _MSC_VER
#define BELLESIP_INLINE __inline
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <inttypes.h>
#define BELLESIP_INLINE inline
#endif

#ifdef _WIN32
#ifdef BELLESIP_EXPORTS
#define BELLESIP_EXPORT __declspec(dllexport)
#define BELLESIP_VAR_EXPORT __declspec(dllexport)
#else
#define BELLESIP_EXPORT
#define BELLESIP_VAR_EXPORT extern __declspec(dllimport)
#endif
#else
#define BELLESIP_VAR_EXPORT extern
#define BELLESIP_EXPORT extern
#endif

#define BELLESIP_UNUSED(a) (void)a;

#undef TRUE
#define TRUE 1


#undef FALSE
#define FALSE 0

#endif
