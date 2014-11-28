############################################################################
# FindLinphone.cmake
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
# - Find the linphone include file and library
#
#  LINPHONE_FOUND - system has linphone
#  LINPHONE_INCLUDE_DIRS - the linphone include directory
#  LINPHONE_LIBRARIES - The libraries needed to use linphone
#  LINPHONE_CPPFLAGS - The compilation flags needed to use linphone
#  LINPHONE_LDFLAGS - The linking flags needed to use linphone

find_package(ORTP REQUIRED)
find_package(MS2 REQUIRED)
find_package(XML2 REQUIRED)
find_package(BelleSIP REQUIRED)

set(_LINPHONE_ROOT_PATHS
	${WITH_LINPHONE}
	${CMAKE_INSTALL_PREFIX}
)

find_path(LINPHONE_INCLUDE_DIRS
	NAMES linphone/linphonecore.h
	HINTS _LINPHONE_ROOT_PATHS
	PATH_SUFFIXES include
)

if(LINPHONE_INCLUDE_DIRS)
	set(HAVE_LINPHONE_LINPHONECORE_H 1)
endif()

find_library(LINPHONE_LIBRARIES
	NAMES linphone
	HINTS ${_LINPHONE_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)

list(APPEND LINPHONE_INCLUDE_DIRS ${ORTP_INCLUDE_DIRS} ${MS2_INCLUDE_DIRS} ${XML2_INCLUDE_DIRS} ${BELLESIP_INCLUDE_DIRS})
list(APPEND LINPHONE_LIBRARIES ${ORTP_LIBRARIES} ${MS2_LIBRARIES} ${XML2_LIBRARIES} ${BELLESIP_LIBRARIES})
if(WIN32)
	list(APPEND LINPHONE_LIBRARIES shlwapi)
endif(WIN32)

list(REMOVE_DUPLICATES LINPHONE_INCLUDE_DIRS)
list(REMOVE_DUPLICATES LINPHONE_LIBRARIES)
set(LINPHONE_CPPFLAGS "${MS2_CPPFLAGS}")
set(LINPHONE_LDFLAGS "${MS2_LDFLAGS} ${BELLESIP_LDFLAGS}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Linphone
	DEFAULT_MSG
	LINPHONE_INCLUDE_DIRS LINPHONE_LIBRARIES
)

mark_as_advanced(LINPHONE_INCLUDE_DIRS LINPHONE_LIBRARIES LINPHONE_CPPFLAGS LINPHONE_LDFLAGS)
