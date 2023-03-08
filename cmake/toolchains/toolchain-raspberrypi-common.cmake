# ##############################################################################
# toolchain-raspberrypi-common.cmake
# Copyright (C) 2023  Belledonne Communications, Grenoble France
# ##############################################################################
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# ##############################################################################

if("$ENV{RASPIOS_SYSROOT_PATH}" STREQUAL "")
  message(FATAL_ERROR "Define the RASPIOS_SYSROOT_PATH environment variable to point to the Raspberry Pi OS sysroot.")
else()
  set(SYSROOT_PATH "$ENV{RASPIOS_SYSROOT_PATH}")
endif()

if("$ENV{RASPBERRY_PI_TOOLCHAIN_PATH}" STREQUAL "")
  message(FATAL_ERROR "Define the RASPBERRY_PI_TOOLCHAIN_PATH environment variable to point to the Raspberry Pi toolchain.")
else()
  set(TOOLCHAIN_PATH "$ENV{RASPBERRY_PI_TOOLCHAIN_PATH}")
endif()

message(STATUS "Using sysroot path: ${SYSROOT_PATH}")


set(CMAKE_SYSROOT "${SYSROOT_PATH}")
set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_LIBRARY_ARCHITECTURE "arm-linux-gnueabihf")


# Define name of the target system
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_VERSION 1)

# Define the compiler
set(BIN_PREFIX "${TOOLCHAIN_PATH}/bin/arm-linux-gnueabihf")
set(CMAKE_C_COMPILER ${BIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${BIN_PREFIX}-g++)
set(CMAKE_LINKER ${BIN_PREFIX}-ld CACHE STRING "Set the cross-compiler tool LD" FORCE)
set(CMAKE_AR ${BIN_PREFIX}-ar CACHE STRING "Set the cross-compiler tool AR" FORCE)
set(CMAKE_NM {BIN_PREFIX}-nm CACHE STRING "Set the cross-compiler tool NM" FORCE)
set(CMAKE_OBJCOPY ${BIN_PREFIX}-objcopy CACHE STRING "Set the cross-compiler tool OBJCOPY" FORCE)
set(CMAKE_OBJDUMP ${BIN_PREFIX}-objdump CACHE STRING "Set the cross-compiler tool OBJDUMP" FORCE)
set(CMAKE_RANLIB ${BIN_PREFIX}-ranlib CACHE STRING "Set the cross-compiler tool RANLIB" FORCE)
set(CMAKE_STRIP {BIN_PREFIX}-strip CACHE STRING "Set the cross-compiler tool RANLIB" FORCE)

# Define the compiler flags
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIC -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE} -L${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE} -L${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE} -L${CMAKE_SYSROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_INSTALL_PREFIX}" "${CMAKE_SYSROOT}")

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# For libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
