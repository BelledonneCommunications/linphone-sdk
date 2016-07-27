############################################################################
# FindArts.txt
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
# - Find the arts include file and library
#
#  ARTS_FOUND - system has arts
#  ARTS_INCLUDE_DIRS - the arts include directory
#  ARTS_LIBRARIES - The libraries needed to use arts

include(CheckSymbolExists)
include(CMakePushCheckState)

set(_ARTS_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(ARTS_INCLUDE_DIRS
	NAMES kde/artsc/artsc.h
	HINTS _ARTS_ROOT_PATHS
	PATH_SUFFIXES include
)
if(ARTS_INCLUDE_DIRS)
	set(HAVE_KDE_ARTSC_ARTSC_H 1)
endif()

find_library(ARTS_LIBRARIES
	NAMES artsc
	HINTS _ARTS_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(ARTS_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${ARTS_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${ARTS_LIBRARIES})
	check_symbol_exists(arts_init "kde/artsc/artsc.h" HAVE_ARTS_INIT)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Arts
	DEFAULT_MSG
	ARTS_INCLUDE_DIRS ARTS_LIBRARIES HAVE_KDE_ARTSC_ARTSC_H HAVE_ARTS_INIT
)

mark_as_advanced(ARTS_INCLUDE_DIRS ARTS_LIBRARIES HAVE_KDE_ARTSC_ARTSC_H HAVE_ARTS_INIT)
