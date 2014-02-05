############################################################################
# bb10-i486.cmake
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
if (NOT UNIX)
	message(FATAL_ERROR "You need to build using a Linux or a Mac OS X system")
endif(NOT UNIX)

if ("$ENV{QNX_HOST}" STREQUAL "")
	message(FATAL_ERROR "Some environment variables need to be defined by using the bbndk-env*.sh delivered with the bbndk.")
endif()

file(GLOB COMPILER_PATH $ENV{QNX_HOST}/usr/bin/i486-pc-nto-qnx*-gcc)
if("${COMPILER_PATH}" STREQUAL "")
	message(FATAL_ERROR "Could not find compiler")
endif()

get_filename_component(COMPILER_NAME ${COMPILER_PATH} NAME)
string(REGEX REPLACE "-gcc$" "" LINPHONE_BUILDER_TOOLCHAIN_PATH ${COMPILER_PATH})
string(REGEX REPLACE "-gcc$" "" LINPHONE_BUILDER_TOOLCHAIN_HOST ${COMPILER_NAME})

set(LINPHONE_BUILDER_TOOLCHAIN_CC "${LINPHONE_BUILDER_TOOLCHAIN_PATH}-gcc")
set(LINPHONE_BUILDER_TOOLCHAIN_CXX "${LINPHONE_BUILDER_TOOLCHAIN_PATH}-g++")
set(LINPHONE_BUILDER_TOOLCHAIN_LD "${LINPHONE_BUILDER_TOOLCHAIN_PATH}-ld")
set(LINPHONE_BUILDER_TOOLCHAIN_AR "${LINPHONE_BUILDER_TOOLCHAIN_PATH}-ar")
set(LINPHONE_BUILDER_TOOLCHAIN_RANLIB "${LINPHONE_BUILDER_TOOLCHAIN_PATH}-ranlib")
set(LINPHONE_BUILDER_TOOLCHAIN_STRIP "${LINPHONE_BUILDER_TOOLCHAIN_PATH}-strip")
set(LINPHONE_BUILDER_TOOLCHAIN_NM "${LINPHONE_BUILDER_TOOLCHAIN_PATH}-nm")

set(LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS "-D_REENTRANT -D__QNXNTO__ -Dasm=__asm")
#-D__PLAYBOOK__
set(LINPHONE_BUILDER_TOOLCHAIN_CFLAGS "-g -fPIC -fstack-protector-strong")
set(LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS "-L$ENV{QNX_TARGET}/x86/lib -Wl,-z,relro -Wl,-z,now -pie -lbps -lsocket")


include(CMakeForceCompiler)

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_PROCESSOR i486)

# Define the compiler
CMAKE_FORCE_C_COMPILER(${LINPHONE_BUILDER_TOOLCHAIN_CC} GNU)
CMAKE_FORCE_CXX_COMPILER(${LINPHONE_BUILDER_TOOLCHAIN_CXX} GNU)

set(CMAKE_FIND_ROOT_PATH  $ENV{QNX_TARGET})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
