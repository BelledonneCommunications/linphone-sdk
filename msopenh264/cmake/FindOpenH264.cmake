############################################################################
# FindOpenH264.cmake
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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
# Find the openh264 library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  openh264 - If the openh264 library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  OpenH264_FOUND - The openh264 library has been found
#  OpenH264_TARGET - The name of the CMake target for the openh264 library


include(FindPackageHandleStandardArgs)

set(_OpenH264_REQUIRED_VARS OpenH264_TARGET)
set(_OpenH264_CACHE_VARS ${_OpenH264_REQUIRED_VARS})

if(TARGET openh264)

	set(OpenH264_TARGET libopenh264)
	set(OpenH264_USE_BUILD_INTERFACE TRUE)

else()

	set(_OpenH264_ROOT_PATHS
		${WITH_OpenH264}
		${CMAKE_INSTALL_PREFIX}
	)

	find_path(_OpenH264_INCLUDE_DIRS
		NAMES wels/codec_api.h
		HINTS ${_OpenH264_ROOT_PATHS}
		PATH_SUFFIXES include
	)

	find_library(_OpenH264_LIBRARY
		NAMES openh264_dll openh264
		HINTS ${_OpenH264_ROOT_PATHS}
		PATH_SUFFIXES bin lib
	)

	if(_OpenH264_INCLUDE_DIRS AND _OpenH264_LIBRARY)
		if(WIN32 OR ANDROID) # Issue with check_cxx_symbol_exists on Windows (TODO: missing libraries in OPENH264_LIBRARIES?). If OPENH264_INCLUDE_DIRS, then decoder exists.
			set(HAVE_WELS_CREATE_DECODER TRUE)
		else()
			include(CMakePushCheckState)
			include(CheckCXXSymbolExists)
			# Need pthread lib for check_cxx_symbol_exists
			set(THREADS_PREFER_PTHREAD_FLAG TRUE)
			find_package(Threads REQUIRED)
			list(APPEND OPENH264_LIBRARIES Threads::Threads)
			cmake_push_check_state(RESET)
			list(APPEND CMAKE_REQUIRED_INCLUDES ${_OpenH264_INCLUDE_DIRS})
			list(APPEND CMAKE_REQUIRED_LIBRARIES ${_OpenH264_LIBRARY})
			check_cxx_symbol_exists("WelsCreateDecoder" "wels/codec_api.h" HAVE_WELS_CREATE_DECODER)
			cmake_pop_check_state()
		endif()

		if(HAVE_WELS_CREATE_DECODER)
			add_library(openh264 UNKNOWN IMPORTED)
			if(WIN32)
			set_target_properties(openh264 PROPERTIES
					INTERFACE_INCLUDE_DIRECTORIES "${_OpenH264_INCLUDE_DIRS}"
					IMPORTED_IMPLIB "${_OpenH264_LIBRARY}"
				)
			else()
			set_target_properties(openh264 PROPERTIES
					INTERFACE_INCLUDE_DIRECTORIES "${_OpenH264_INCLUDE_DIRS}"
					IMPORTED_LOCATION "${_OpenH264_LIBRARY}"
				)
			endif()
			set(OpenH264_TARGET openh264)
		endif()
	endif()

endif()

find_package_handle_standard_args(OpenH264
	REQUIRED_VARS ${_OpenH264_REQUIRED_VARS}
)
mark_as_advanced(${_OpenH264_CACHE_VARS})
