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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
if("${BBNDK_ENV_SCRIPT}" STREQUAL "")
	message(FATAL_ERROR "Could not find the bbndk environment setup script. Please make sure you installed the BlackBerry 10 native SDK.")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/toolchains/bb10/get_qnx_host.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/get_qnx_host.sh)
execute_process(COMMAND ${CMAKE_CURRENT_BINARY_DIR}/get_qnx_host.sh
	OUTPUT_VARIABLE QNX_HOST
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(GLOB COMPILER_PATH "${QNX_HOST}/usr/bin/${COMPILER_PREFIX}*-gcc")
if("${COMPILER_PATH}" STREQUAL "")
	message(FATAL_ERROR "Could not find compiler")
endif()

get_filename_component(COMPILER_NAME ${COMPILER_PATH} NAME)
string(REGEX REPLACE "-gcc$" "" LINPHONE_BUILDER_TOOLCHAIN_PATH ${COMPILER_PATH})
string(REGEX REPLACE "-gcc$" "" LINPHONE_BUILDER_TOOLCHAIN_HOST ${COMPILER_NAME})

foreach(TOOLNAME gcc g++ ld ar ranlib strip nm)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/toolchains/bb10/tool_wrapper.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/${TOOLNAME}.sh)
endforeach(TOOLNAME)

set(LINPHONE_BUILDER_TOOLCHAIN_CC "${CMAKE_CURRENT_BINARY_DIR}/gcc.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_CXX "${CMAKE_CURRENT_BINARY_DIR}/g++.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_LD "${CMAKE_CURRENT_BINARY_DIR}/ld.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_AR "${CMAKE_CURRENT_BINARY_DIR}/ar.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_RANLIB "${CMAKE_CURRENT_BINARY_DIR}/ranlib.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_STRIP "${CMAKE_CURRENT_BINARY_DIR}/strip.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_NM "${CMAKE_CURRENT_BINARY_DIR}/nm.sh")

set(LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS "-D_REENTRANT -D__QNXNTO__ -Dasm=__asm")
set(LINPHONE_BUILDER_TOOLCHAIN_CFLAGS "-fPIC -fstack-protector-strong")
set(LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS "-L$ENV{QNX_TARGET}/x86/lib -Wl,-z,relro -Wl,-z,now -pie -lbps -lsocket -lslog2")


include(CMakeForceCompiler)

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR ${SYSTEM_PROCESSOR})

# Define the compiler
CMAKE_FORCE_C_COMPILER(${LINPHONE_BUILDER_TOOLCHAIN_CC} GNU)
CMAKE_FORCE_CXX_COMPILER(${LINPHONE_BUILDER_TOOLCHAIN_CXX} GNU)

set(CMAKE_FIND_ROOT_PATH $ENV{QNX_TARGET} ${CMAKE_INSTALL_PREFIX})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Restrict pkg-config to search in the install directory
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
