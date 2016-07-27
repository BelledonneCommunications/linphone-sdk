############################################################################
# FindXv.txt
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
# - Find the Xv include file and library
#
#  XV_FOUND - system has Xv
#  XV_INCLUDE_DIRS - the Xv include directory
#  XV_LIBRARIES - The libraries needed to use Xv

include(CheckIncludeFile)
include(CheckSymbolExists)

find_package(X11 REQUIRED)

set(_XV_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(XV_H_INCLUDE_DIR
	NAMES X11/extensions/Xv.h
	HINTS _XV_ROOT_PATHS
	PATH_SUFFIXES include
)
find_path(XVLIB_H_INCLUDE_DIR
	NAMES X11/extensions/Xvlib.h
	HINTS _XV_ROOT_PATHS
	PATH_SUFFIXES include
)
if(XV_H_INCLUDE_DIR)
	set(HAVE_X11_EXTENSIONS_XV_H 1)
endif()
if(XVLIB_H_INCLUDE_DIR)
	set(HAVE_X11_EXTENSIONS_XVLIB_H 1)
endif()
if(XV_H_INCLUDE_DIR AND XVLIB_H_INCLUDE_DIR)
	set(XV_INCLUDE_DIRS ${XV_H_INCLUDE_DIR} ${XVLIB_H_INCLUDE_DIR})
endif()

set(XV_INCLUDE_DIRS ${XV_INCLUDE_DIRS} ${X11_INCLUDE_DIRS})
list(REMOVE_DUPLICATES XV_INCLUDE_DIRS)

find_library(XV_LIBRARIES
	NAMES Xv
	HINTS _XV_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(XV_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${XV_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${XV_LIBRARIES})
	check_symbol_exists(XvCreateImage "X11/Xlib.h;X11/extensions/Xv.h;X11/extensions/Xvlib.h" HAVE_XV_CREATE_IMAGE)
	cmake_pop_check_state()
endif()

set(XV_LIBRARIES ${XV_LIBRARIES} ${X11_LIBRARIES})
list(REMOVE_DUPLICATES XV_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Xv
	DEFAULT_MSG
	XV_INCLUDE_DIRS HAVE_X11_EXTENSIONS_XV_H HAVE_X11_EXTENSIONS_XVLIB_H XV_LIBRARIES HAVE_XV_CREATE_IMAGE
)

mark_as_advanced(XV_INCLUDE_DIRS HAVE_X11_EXTENSIONS_XV_H HAVE_X11_EXTENSIONS_XVLIB_H XV_LIBRARIES HAVE_XV_CREATE_IMAGE)
