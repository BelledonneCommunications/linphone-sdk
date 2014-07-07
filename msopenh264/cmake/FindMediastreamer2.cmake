############################################################################
# FindMediastreamer2.txt
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
# - Find the mediastreamer2 include file and library
#
#  MS2_FOUND - system has mediastreamer2
#  MS2_INCLUDE_DIRS - the mediastreamer2 include directory
#  MS2_LIBRARIES - The libraries needed to use mediastreamer2

if(UNIX)
	find_package(PkgConfig)
	pkg_check_modules(_CUNIT QUIET cunit)
endif()

set(_MS2_ROOT_PATHS
	${WITH_MS2}
	${CMAKE_INSTALL_PREFIX}
)

find_path(MS2_INCLUDE_DIR
	NAMES mediastreamer2/mscommon.h
	HINTS _MS2_ROOT_PATHS
	PATH_SUFFIXES include
)

if(NOT "${MS2_INCLUDE_DIR}" STREQUAL "")
	set(HAVE_MEDIASTREAMER2_MSCOMMON_H 1)

	find_library(MS2_BASE_LIBRARIES
		NAMES mediastreamer_base
		HINTS ${_MS2_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)
	find_library(MS2_VOIP_LIBRARIES
		NAMES mediastreamer_voip
		HINTS ${_MS2_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)
	set(MS2_LIBRARIES ${MS2_BASE_LIBRARIES} ${MS2_VOIP_LIBRARIES})

	if(NOT "${MS2_LIBRARIES}" STREQUAL "")
		set(MS2_FOUND TRUE)
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(mediastreamer2
	REQUIRED_VARS
		MS2_LIBRARIES
		MS2_INCLUDE_DIR
	FAIL_MESSAGE
		"Could NOT find mediastreamer2"
)

mark_as_advanced(MS2_INCLUDE_DIR MS2_LIBRARIES)
