############################################################################
# toolchan-bb10.cmake
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

# Building for BlackBerry is only available under UNIX systems
if(NOT UNIX)
	message(FATAL_ERROR "You need to build using a Linux or a Mac OS X system")
endif(NOT UNIX)

if(APPLE)
	set(BBNDK_SEARCH_PATH /Applications/Momentics.app)
else(APPLE)
	set(BBNDK_SEARCH_PATH $ENV{HOME}/bbndk)
endif(APPLE)
file(GLOB BBNDK_ENV_SCRIPT ${BBNDK_SEARCH_PATH}/bbndk-env*.sh)
if(BBNDK_ENV_SCRIPT STREQUAL "")
	message(FATAL_ERROR "Could not find the bbndk environment setup script. Please make sure you installed the BlackBerry 10 native SDK.")
endif()

configure_file(${CMAKE_CURRENT_LIST_DIR}/get_qnx_host.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/get_qnx_host.sh)
execute_process(COMMAND ${CMAKE_CURRENT_BINARY_DIR}/get_qnx_host.sh
	OUTPUT_VARIABLE QNX_HOST
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
configure_file(${CMAKE_CURRENT_LIST_DIR}/get_qnx_target.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/get_qnx_target.sh)
execute_process(COMMAND ${CMAKE_CURRENT_BINARY_DIR}/get_qnx_target.sh
	OUTPUT_VARIABLE QNX_TARGET
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(GLOB COMPILER_PATH "${QNX_HOST}/usr/bin/${COMPILER_PREFIX}*-gcc")
if(COMPILER_PATH STREQUAL "")
	message(FATAL_ERROR "Could not find compiler")
endif()

get_filename_component(COMPILER_NAME ${COMPILER_PATH} NAME)
string(REGEX REPLACE "-gcc$" "" TOOLCHAIN_PATH ${COMPILER_PATH})
string(REGEX REPLACE "-gcc$" "" TOOLCHAIN_HOST ${COMPILER_NAME})

if("${SYSTEM_PROCESSOR}" STREQUAL "arm")
SET(arch gcc_ntoarmv7le)
else()
SET(arch gcc_ntox86)
endif()

foreach(TOOLNAME gcc g++ ld)
	SET(TOOLPATH "${QNX_HOST}/usr/bin/qcc -V${arch}")
	configure_file(${CMAKE_CURRENT_LIST_DIR}/tool_wrapper.cmake ${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-${TOOLNAME})
endforeach(TOOLNAME)
foreach(TOOLNAME ar ranlib strip nm as)
	set(TOOLPATH "${TOOLCHAIN_PATH}-${TOOLNAME}")
	configure_file(${CMAKE_CURRENT_LIST_DIR}/tool_wrapper.cmake ${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-${TOOLNAME})
endforeach(TOOLNAME)

set(TOOLCHAIN_CC "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-gcc")
set(TOOLCHAIN_CXX "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-g++")
set(TOOLCHAIN_LD "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-ld")
set(TOOLCHAIN_AR "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-ar")
set(TOOLCHAIN_RANLIB "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-ranlib")
set(TOOLCHAIN_STRIP "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-strip")
set(TOOLCHAIN_NM "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-nm")
set(TOOLCHAIN_AS "${CMAKE_CURRENT_BINARY_DIR}/${TOOLCHAIN_HOST}-as")

set(QNX True)

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR ${SYSTEM_PROCESSOR})

# Define the compiler
set(CMAKE_C_COMPILER "${TOOLCHAIN_CC}")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_CXX}")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_INSTALL_PREFIX} ${QNX_TARGET} ${QNX_TARGET}/${ROOT_PATH_SUFFIX})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
