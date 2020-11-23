/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#ifndef _FD_PORTAB_H
#define _FD_PORTAB_H

/*
 * This header allows the usage of POSIX file manipulation API on WIN32 platforms.
 * Since all the functions are defined by macro definitions,
 * NEVER INCLUDE THIS FILE IN ANOTHER HEADER and ALWAYS INCLUDE IT AT THE LAST OF THE INCLUSION SEQUENCE
 * or you may encouter linker problem on WIN32.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef _WIN32
#	include <io.h>
#	ifndef R_OK
#		define R_OK 0x2
#	endif
#	ifndef W_OK
#		define W_OK 0x6
#	endif
#   ifndef F_OK
#       define F_OK 0x0
#   endif

#	ifndef S_IRUSR
#	define S_IRUSR S_IREAD
#	endif

#	ifndef S_IWUSR
#	define S_IWUSR S_IWRITE
#	endif

#	define open _open
#	define read _read
#	define write _write
#	define close _close
#	define access _access
#	define lseek _lseek
#else /*_WIN32*/

#	ifndef O_BINARY
#	define O_BINARY 0
#	endif

#endif /*!_WIN32*/

#endif // #ifndef _FD_PORTAB_H
