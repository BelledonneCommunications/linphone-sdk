/***************************************************************************
* config.h.cmake
* Copyright (C) 2014  Belledonne Communications, Grenoble France
*
****************************************************************************
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
****************************************************************************/

#ifndef CONFIG_H
#define CONFIG_H

#define OPUS_BUILD 1

/* Enable SSE functions, if compiled with SSE/SSE2 (note that AMD64 implies SSE2) */
#if defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 1))
#define __SSE__ 1
#endif

#cmakedefine VAR_ARRAYS 1
#cmakedefine USE_ALLOCA 1
#cmakedefine FIXED_POINT 1
#cmakedefine FIXED_DEBUG 1
#cmakedefine DISABLE_FLOAT_API 1
#cmakedefine CUSTOM_MODES 1
#cmakedefine FLOAT_APPROX 1
#cmakedefine OPUS_ARM_ASM 1
#cmakedefine OPUS_ARM_INLINE_ASM 1
#cmakedefine OPUS_ARM_INLINE_EDSP 1
#cmakedefine OPUS_ARM_INLINE_MEDIA 1
#cmakedefine OPUS_ARM_INLINE_NEON 1
#cmakedefine OPUS_ARM_MAY_HAVE_EDSP 1
#cmakedefine OPUS_ARM_PRESUME_EDSP 1
#cmakedefine OPUS_ARM_MAY_HAVE_MEDIA 1
#cmakedefine OPUS_ARM_PRESUME_MEDIA 1
#cmakedefine OPUS_ARM_MAY_HAVE_NEON 1
#cmakedefine OPUS_ARM_PRESUME_NEON 1
#cmakedefine OPUS_HAVE_RTCD 1
#cmakedefine ENABLE_ASSERTIONS 1
#cmakedefine FUZZING 1

#define PACKAGE_VERSION "1.1"

#endif /* CONFIG_H */
