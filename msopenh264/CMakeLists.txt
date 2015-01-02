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

cmake_minimum_required(VERSION 2.8)
project(MSOPENH264 C)
enable_language(CXX)


set(PACKAGE "msopenh264")
set(PACKAGE_NAME "${PACKAGE}")
set(PACKAGE_VERSION "0.1.0")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "support@belledonne-communications.com")
set(PACKAGE_TARNAME "msopenh264")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_DECODER "Enable the openh264 decoder" Yes)
option(ENABLE_STATIC "Build static library (default is shared library)." NO)
set(WITH_MS2 "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where the mediastreamer libraries can be found (ex:/usr or /usr/local)")
set(WITH_OPENH264 "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where libopenh264 can be found (ex:/usr or /usr/local)")
set(WITH_ORTP "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where the oRTP library can be found (ex:/usr or /usr/local)")


if(MSVC)
	include_directories(${CMAKE_INSTALL_PREFIX}/include/MSVC)
endif()


list(APPEND CMAKE_MODULE_PATH ${CMAKE_PREFIX_PATH}/share/cmake/Modules ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CheckIncludeFile)
check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("memory.h" HAVE_MEMORY_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("stdbool.h" HAVE_STDBOOL_H)

find_package(ORTP)
find_package(MS2)
find_package(OpenH264)

if(NOT ORTP_FOUND)
	message(FATAL "Could not find oRTP headers/library!")
endif()
if(NOT MS2_FOUND)
	message(FATAL "Could not find mediastreamer2 headers/library!")
endif()
if(NOT OPENH264_FOUND)
	message(FATAL "Could not find openh264 headers/library!")
endif()


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


include_directories(
	${MS2_INCLUDE_DIRS}
	${OPENH264_INCLUDE_DIRS}
 	src
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}/src
)

add_definitions(-DHAVE_CONFIG_H)
if(NOT MSVC)
	add_definitions(-Wall -Wno-error=unknown-pragmas)
	if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
		add_definitions(-Werror -Wno-error=unknown-warning-option)
	elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		add_definitions(-Werror -Wno-error=pragmas)
	endif()
endif(NOT MSVC)

add_subdirectory(src)
