/***************************************************************************
* antlr3config.h.cmake
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

#cmakedefine ANTLR3_NODEBUGGER
#cmakedefine ANTLR3_USE_64BIT

#cmakedefine HAVE_ARPA_NAMESER_H 1
#cmakedefine HAVE_CTYPE_H 1
#cmakedefine HAVE_INTTYPES_H 1
#cmakedefine HAVE_MALLOC_H 1
#cmakedefine HAVE_MEMORY_H 1
#cmakedefine HAVE_NETDB_H 1
#cmakedefine HAVE_NETINET_IN_H 1
#cmakedefine HAVE_NETINET_TCP_H 1
#cmakedefine HAVE_SOCKET_H 1
#cmakedefine HAVE_STDARG_H 1
#cmakedefine HAVE_STDINT_H 1
#cmakedefine HAVE_STDLIB_H 1
#cmakedefine HAVE_STRINGS_H 1
#cmakedefine HAVE_STRING_H 1
#cmakedefine HAVE_SYS_MALLOC_H 1
#cmakedefine HAVE_SYS_SOCKET_H 1
#cmakedefine HAVE_SYS_STAT_H 1
#cmakedefine HAVE_SYS_TYPES_H 1
#cmakedefine HAVE_UNISTD_H 1

#ifndef __cplusplus
#cmakedefine inline @inline@
#endif
