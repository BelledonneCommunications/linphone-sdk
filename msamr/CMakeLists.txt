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
project(MSAMR C)


set(PACKAGE "msamr")
set(PACKAGE_NAME "${PACKAGE}")
set(PACKAGE_VERSION "1.0.1")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "support@belledonne-communications.com")
set(PACKAGE_TARNAME "msamr")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_NARROWBAND "Enable AMR narrowband." Yes)
option(ENABLE_WIDEBAND "Enable AMR wideband." No)
option(ENABLE_STATIC "Build static library (default is shared library)." NO)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_ms2_CONFIG_DIR}/Mediastreamer2Config.cmake")
else()
	find_package(Mediastreamer2 REQUIRED)
endif()
if(ENABLE_NARROWBAND)
	find_package(OpenCoreAMRNB REQUIRED)
endif()
if(ENABLE_WIDEBAND)
	find_package(OpenCoreAMRWB REQUIRED)
	find_package(VoAMRWBEnc REQUIRED)
endif()


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


include_directories(
	${MEDIASTREAMER2_INCLUDE_DIRS}
	src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/src
)
if(ENABLE_NARROWBAND)
	include_directories(${OPENCOREAMRNB_INCLUDE_DIRS})
	add_definitions("-DHAVE_AMRNB=1")
endif()
if(ENABLE_WIDEBAND)
	include_directories(
		${OPENCOREMARWB_INCLUDE_DIRS}
		${VOAMRWBENC_INCLUDE_DIRS}
	)
	add_definitions("-DHAVE_AMRWB=1")
endif()
set(MSVC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/MSVC")
if(MSVC)
	include_directories(${MSVC_INCLUDE_DIR})
endif()

add_definitions("-DHAVE_CONFIG_H")
if(NOT MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-error=unknown-pragmas")
	if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-error=unknown-warning-option")
	elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-error=pragmas")
	endif()
endif()

add_subdirectory(src)

