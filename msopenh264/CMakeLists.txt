############################################################################
# CMakeLists.txt
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

cmake_minimum_required(VERSION 3.0)
project(MSOPENH264)


set(PACKAGE "msopenh264")
set(PACKAGE_NAME "${PACKAGE}")
set(PACKAGE_VERSION "1.1.1")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "support@belledonne-communications.com")
set(PACKAGE_TARNAME "msopenh264")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_DECODER "Enable the openh264 decoder" Yes)
option(ENABLE_STATIC "Build static library (default is shared library)." NO)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CheckIncludeFile)
check_include_file("stdbool.h" HAVE_STDBOOL_H)

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_ms2_CONFIG_DIR}/Mediastreamer2Config.cmake")
else()
	find_package(Mediastreamer2 REQUIRED)
endif()
find_package(OpenH264 REQUIRED)


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


include_directories(
	${MEDIASTREAMER2_INCLUDE_DIRS}
	${OPENH264_INCLUDE_DIRS}
	src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/src
)

add_definitions(-DHAVE_CONFIG_H)
if(NOT MSVC)
	add_definitions(-Wall -Wno-error=unknown-pragmas)
	if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
		add_definitions("-Wno-error=unknown-warning-option")
		if (NOT IOS)
			add_definitions("-Werror")
		endif()
	elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		add_definitions("-Werror -Wno-error=pragmas")
	endif()
endif(NOT MSVC)

add_subdirectory(src)
