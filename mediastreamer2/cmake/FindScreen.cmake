############################################################################
# FindQSA.txt
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################
#
# - Find the libscreen include file and library
#
#  SCREEN_FOUND - system has libscreen
#  SCREEN_INCLUDE_DIRS - the libscreen include directory
#  SCREEN_LIBRARIES - The libraries needed to use libscreen

include(CheckSymbolExists)
include(CMakePushCheckState)

set(_SCREEN_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(SCREEN_INCLUDE_DIRS
	NAMES screen/screen.h
	HINTS _SCREEN_ROOT_PATHS
	PATH_SUFFIXES include
)

find_library(SCREEN_LIBRARIES
	NAMES screen
	HINTS _SCREEN_ROOT_PATHS
	PATH_SUFFIXES lib
)

if(SCREEN_LIBRARIES)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${SCREEN_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${SCREEN_LIBRARIES})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SCREEN
	DEFAULT_MSG
	SCREEN_INCLUDE_DIRS SCREEN_LIBRARIES
)

mark_as_advanced(SCREEN_INCLUDE_DIRS SCREEN_LIBRARIES)
