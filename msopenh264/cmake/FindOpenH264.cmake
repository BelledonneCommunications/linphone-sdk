############################################################################
# FindOpenH264.txt
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
# - Find the openh264 include file and library
#
#  OPENH264_FOUND - system has openh264
#  OPENH264_INCLUDE_DIRS - the openh264 include directory
#  OPENH264_LIBRARIES - The libraries needed to use openh264

include(CMakePushCheckState)
include(CheckCXXSymbolExists)

set(_OPENH264_ROOT_PATHS
	${WITH_OPENH264}
	${CMAKE_INSTALL_PREFIX}
)

find_path(OPENH264_INCLUDE_DIRS
	NAMES wels/codec_api.h
	HINTS _OPENH264_ROOT_PATHS
	PATH_SUFFIXES include
)
if(OPENH264_INCLUDE_DIRS)
	set(HAVE_WELS_CODEC_API_H 1)
endif()

find_library(OPENH264_LIBRARIES
	NAMES openh264 openh264_dll
	HINTS _OPENH264_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

if(OPENH264_LIBRARIES)
	if( NOT WIN32 )
		find_library(PTHREAD_LIBRARY
			NAMES pthread
		)
		list(APPEND OPENH264_LIBRARIES ${PTHREAD_LIBRARY})
	endif()
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${OPENH264_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${OPENH264_LIBRARIES})
	check_cxx_symbol_exists("WelsCreateDecoder" "wels/codec_api.h" HAVE_WELS_CREATE_DECODER)
	cmake_pop_check_state()
	if(HAVE_WELS_CREATE_DECODER)
		set(OPENH264_FOUND TRUE)
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenH264
	DEFAULT_MSG
	OPENH264_INCLUDE_DIRS OPENH264_LIBRARIES HAVE_WELS_CODEC_API_H HAVE_WELS_CREATE_DECODER
)

mark_as_advanced(OPENH264_INCLUDE_DIRS OPENH264_LIBRARIES HAVE_WELS_CODEC_API_H HAVE_WELS_CREATE_DECODER)
