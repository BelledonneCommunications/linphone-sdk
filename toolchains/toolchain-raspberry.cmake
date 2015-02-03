############################################################################
# toolchain-raspberry.cmake
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

if("$ENV{RASPBIAN_ROOTFS}" STREQUAL "")
	message(FATAL_ERROR "Define the RASPBIAN_ROOTFS environment variable to point to the raspbian rootfs.")
else()
	set(SYSROOT_PATH "$ENV{RASPBIAN_ROOTFS}")
endif()
set(RASPBERRY_TOOLCHAIN_HOST "arm-linux-gnueabihf")

message(STATUS "Using sysroot path: ${SYSROOT_PATH}")

set(RASPBERRY_TOOLCHAIN_CC "${RASPBERRY_TOOLCHAIN_HOST}-gcc")
set(RASPBERRY_TOOLCHAIN_CXX "${RASPBERRY_TOOLCHAIN_HOST}-g++")
set(RASPBERRY_TOOLCHAIN_LD "${RASPBERRY_TOOLCHAIN_HOST}-ld")
set(RASPBERRY_TOOLCHAIN_AR "${RASPBERRY_TOOLCHAIN_HOST}-ar")
set(RASPBERRY_TOOLCHAIN_RANLIB "${RASPBERRY_TOOLCHAIN_HOST}-ranlib")
set(RASPBERRY_TOOLCHAIN_STRIP "${RASPBERRY_TOOLCHAIN_HOST}-strip")
set(RASPBERRY_TOOLCHAIN_NM "${RASPBERRY_TOOLCHAIN_HOST}-nm")


include(CMakeForceCompiler)

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR "arm")

# Define the compiler
CMAKE_FORCE_C_COMPILER(${RASPBERRY_TOOLCHAIN_CC} GNU)
CMAKE_FORCE_CXX_COMPILER(${RASPBERRY_TOOLCHAIN_CXX} GNU)

set(CMAKE_FIND_ROOT_PATH ${SYSROOT_PATH} ${CMAKE_INSTALL_PREFIX})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
