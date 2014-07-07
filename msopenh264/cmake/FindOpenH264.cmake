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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################
#
# - Find the openh264 include file and library
#
#  OPENH264_FOUND - system has openh264
#  OPENH264_INCLUDE_DIR - the openh264 include directory
#  OPENH264_LIBRARIES - The libraries needed to use openh264

if("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
	include(CMakePushCheckState)
endif("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
include(CheckLibraryExists)

set(_OPENH264_ROOT_PATHS
	${WITH_OPENH264}
	${CMAKE_INSTALL_PREFIX}
)

find_path(OPENH264_INCLUDE_DIR
	NAMES wels/codec_api.h
	HINTS _OPENH264_ROOT_PATHS
	PATH_SUFFIXES include
)
if(NOT "${OPENH264_INCLUDE_DIR}" STREQUAL "")
	set(HAVE_WELS_CODEC_API_H 1)

	find_library(OPENH264_LIBRARIES
		NAMES openh264
		HINTS _OPENH264_ROOT_PATHS
		PATH_SUFFIXES bin lib
	)

	if(NOT "${OPENH264_LIBRARIES}" STREQUAL "")
		if("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
			cmake_push_check_state(RESET)
		else()
			set(SAVE_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
			set(SAVE_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
		endif()
		set(CMAKE_REQUIRED_INCLUDES ${OPENH264_INCLUDE_DIR})
		set(CMAKE_REQUIRED_LIBRARIES ${OPENH264_LIBRARIES})
		check_library_exists(${OPENH264_LIBRARIES} "WelsCreateDecoder" "" HAVE_WELS_CREATE_DECODER)
		if("${CMAKE_VERSION}" VERSION_GREATER "2.8.5")
			cmake_pop_check_state()
		else()
			set(CMAKE_REQUIRED_INCLUDES ${SAVE_CMAKE_REQUIRED_INCLUDES})
			set(CMAKE_REQUIRED_LIBRARIES ${SAVE_CMAKE_REQUIRED_LIBRARIES})
		endif()
		if(HAVE_WELS_CREATE_DECODER)
			set(OPENH264_FOUND TRUE)
		endif()
	endif()
endif()

mark_as_advanced(OPENH264_INCLUDE_DIR OPENH264_LIBRARIES)
