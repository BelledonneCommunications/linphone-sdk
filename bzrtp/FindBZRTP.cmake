############################################################################
# FindBZRTP.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################
#
# - Find the bzrtp include file and library
#
#  BZRTP_FOUND - system has bzrtp
#  BZRTP_INCLUDE_DIRS - the bzrtp include directory
#  BZRTP_LIBRARIES - The libraries needed to use bzrtp

find_package(PolarSSL REQUIRED)
find_package(XML2)

set(_BZRTP_ROOT_PATHS
	${WITH_BZRTP}
	${CMAKE_INSTALL_PREFIX}
)

find_path(BZRTP_INCLUDE_DIRS
	NAMES bzrtp/bzrtp.h
	HINTS _BZRTP_ROOT_PATHS
	PATH_SUFFIXES include
)

if(BZRTP_INCLUDE_DIRS)
	set(HAVE_BZRTP_BZRTP_H 1)
endif()

find_library(BZRTP_LIBRARIES
	NAMES bzrtp
	HINTS ${_BZRTP_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)

list(APPEND BZRTP_INCLUDE_DIRS ${POLARSSL_INCLUDE_DIRS})
list(APPEND BZRTP_LIBRARIES ${POLARSSL_LIBRARIES})
if(XML2_FOUND)
	list(APPEND BZRTP_INCLUDE_DIRS ${XML2_INCLUDE_DIRS})
	list(APPEND BZRTP_LIBRARIES ${XML2_LIBRARIES})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(BZRTP
	DEFAULT_MSG
	BZRTP_INCLUDE_DIRS BZRTP_LIBRARIES
)

mark_as_advanced(BZRTP_INCLUDE_DIRS BZRTP_LIBRARIES)
