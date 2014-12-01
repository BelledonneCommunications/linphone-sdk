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
project(MSAMR C)


set(PACKAGE "msamr")
set(PACKAGE_NAME "${PACKAGE}")
set(PACKAGE_VERSION "0.0.2")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "support@belledonne-communications.com")
set(PACKAGE_TARNAME "msamr")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_NARROWBAND "Enable AMR narrowband." Yes)
option(ENABLE_WIDEBAND "Enable AMR wideband." No)
set(WITH_MS2 "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where the mediastreamer libraries can be found (ex:/usr or /usr/local)")
set(WITH_OPENCOREAMR "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where libopencoreamr can be found (ex:/usr or /usr/local)")
set(WITH_ORTP "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where the oRTP library can be found (ex:/usr or /usr/local)")
set(WITH_VOAMRWBENC "${CMAKE_INSTALL_PREFIX}" CACHE STRING "Set prefix where libvo-amrwbenc can be found (ex:/usr or /usr/local)")


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

find_package(ORTP)
find_package(MS2)
if(NOT ORTP_FOUND)
	message(FATAL "Could not find oRTP headers/library!")
endif()
if(NOT MS2_FOUND)
	message(FATAL "Could not find mediastreamer2 headers/library!")
endif()

if(ENABLE_NARROWBAND)
	find_package(OpenCoreAMRNB)
	if(NOT OPENCOREAMRNB_FOUND)
		message(FATAL "Could not find opencoreamrnb headers/library!")
	endif()
endif()
if(ENABLE_WIDEBAND)
	find_package(OpenCoreAMRWB)
	if(NOT OPENCOREAMRWB_FOUND)
		message(FATAL "Could not find opencoreamrwb headers/library!")
	endif()
	find_package(VoAMRWBEnc)
	if(NOT VOAMRWBENC_FOUND)
		message(FATAL "Could not find vo-amrwbenc headers/library!")
	endif()
endif()


configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


include_directories(
	${MS2_INCLUDE_DIRS}
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

add_definitions("-DHAVE_CONFIG_H")
if(NOT MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-error=unknown-pragmas")
	if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-error=unknown-warning-option")
	elseif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wno-error=pragmas")
	endif()
endif(NOT MSVC)

add_subdirectory(src)
