############################################################################
# FindLibYUV.cmake
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
# This file is part of mediastreamer2.
############################################################################
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
############################################################################
#
# Find the yuv library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  yuv - If the yuv library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  LibYUV_FOUND - The yuv library has been found
#  LibYUV_TARGET - The name of the CMake target for the yuv library


include(FindPackageHandleStandardArgs)

set(_LibYUV_REQUIRED_VARS LibYUV_TARGET HAVE_LIBYUV_H)
set(_LibYUV_CACHE_VARS ${_LibYUV_REQUIRED_VARS})

if(TARGET yuv)

	set(LibYUV_TARGET yuv)
	set(HAVE_LIBYUV_H 1)

else()

	set(_LibYUV_ROOT_PATHS ${CMAKE_INSTALL_PREFIX})

	find_path(_LibYUV_INCLUDE_DIRS
		NAMES libyuv.h
		HINTS ${_LibYUV_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	find_library(_LibYUV_LIBRARY
		NAMES yuv
		HINTS ${_LibYUV_ROOT_PATHS}
		PATH_SUFFIXES bin lib lib/Win32
	)

	if(_LibYUV_INCLUDE_DIRS AND _LibYUV_LIBRARY)
		add_library(yuv UNKNOWN IMPORTED)
		if(WIN32)
			set_target_properties(yuv PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_LibYUV_INCLUDE_DIRS}"
				IMPORTED_IMPLIB "${_LibYUV_LIBRARY}"
			)
		else()
			set_target_properties(yuv PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_LibYUV_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_LibYUV_LIBRARY}"
			)
		endif()
		set(HAVE_LIBYUV_H 1)
		set(LibYUV_TARGET yuv)
	endif()

endif()

find_package_handle_standard_args(LibYUV
	REQUIRED_VARS ${_LibYUV_REQUIRED_VARS}
)
mark_as_advanced(${_LibYUV_CACHE_VARS})
