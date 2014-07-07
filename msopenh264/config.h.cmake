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
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
****************************************************************************/

#cmakedefine PACKAGE "@PACKAGE@"
#cmakedefine PACKAGE_NAME "@PACKAGE_NAME@"
#cmakedefine PACKAGE_VERSION "@PACKAGE_VERSION@"
#cmakedefine PACKAGE_STRING "@PACKAGE_STRING@"
#cmakedefine PACKAGE_TARNAME "@PACKAGE_TARNAME@"
#cmakedefine PACKAGE_BUGREPORT "@PACKAGE_BUGREPORT@"
#cmakedefine PACKAGE_URL "@PACKAGE_URL@"
#cmakedefine VERSION "@VERSION@"

#ifndef HAVE_DLFCN_H
#cmakedefine HAVE_DLFCN_H
#endif
#ifndef HAVE_INTTYPES_H
#cmakedefine HAVE_INTTYPES_H
#endif
#ifndef HAVE_MEMORY_H
#cmakedefine HAVE_MEMORY_H
#endif
#ifndef HAVE_STDINT_H
#cmakedefine HAVE_STDINT_H
#endif
#ifndef HAVE_STDLIB_H
#cmakedefine HAVE_STDLIB_H
#endif
#ifndef HAVE_STRINGS_H
#cmakedefine HAVE_STRINGS_H
#endif
#ifndef HAVE_STRING_H
#cmakedefine HAVE_STRING_H
#endif
#ifndef HAVE_SYS_STAT_H
#cmakedefine HAVE_SYS_STAT_H
#endif
#ifndef HAVE_SYS_TYPES_H
#cmakedefine HAVE_SYS_TYPES_H
#endif
#ifndef HAVE_UNISTD_H
#cmakedefine HAVE_UNISTD_H
#endif
#ifndef HAVE_STDBOOL_H
#cmakedefine HAVE_STDBOOL_H
#endif

#cmakedefine HAVE_MEDIASTREAMER2_MSCOMMON_H

#cmakedefine HAVE_WELS_CODEC_API_H
#cmakedefine HAVE_WELS_CREATE_DECODER
