############################################################################
# toolchain-ios.cmake
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

# Building for iOS is only available under APPLE systems
if(NOT APPLE)
	message(FATAL_ERROR "You need to build using a Mac OS X system")
endif()

execute_process(COMMAND xcode-select -print-path OUTPUT_VARIABLE XCODE_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT ${XCODE_SELECT_RESULT} EQUAL 0)
	message(FATAL_ERROR "xcode-select failed: ${XCODE_SELECT_RESULT}. You may need to install Xcode.")
endif()

if(EXISTS "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/")
	# New path with Xcode 4.3
	file(GLOB SDK_PATH_LIST "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/iPhone${PLATFORM}*.sdk")
	set(SDK_BIN_PATH "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/usr/bin")
else()
	file(GLOB SDK_PATH_LIST "/Developer/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/iPhone${PLATFORM}*.sdk")
	set(SDK_BIN_PATH "/Developer/Platforms/iPhone${PLATFORM}.platform/Developer/usr/bin")
endif()
list(SORT SDK_PATH_LIST)
list(REVERSE SDK_PATH_LIST)
list(GET SDK_PATH_LIST 0 CMAKE_OSX_SYSROOT)
message(STATUS "Using sysroot path: ${CMAKE_OSX_SYSROOT}")

set(IOS_TOOLCHAIN_HOST ${COMPILER_PREFIX})

foreach(TOOLNAME clang clang++ ld ar ranlib strip nm)
	configure_file(${CMAKE_CURRENT_LIST_DIR}/tool_wrapper.cmake ${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-${TOOLNAME})
endforeach(TOOLNAME)

set(IOS_TOOLCHAIN_CC "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-clang")
set(IOS_TOOLCHAIN_CXX "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-clang++")
set(IOS_TOOLCHAIN_OBJC "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-clang")
set(IOS_TOOLCHAIN_LD "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-ld")
set(IOS_TOOLCHAIN_AR "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-ar")
set(IOS_TOOLCHAIN_RANLIB "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-ranlib")
set(IOS_TOOLCHAIN_STRIP "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-strip")
set(IOS_TOOLCHAIN_NM "${CMAKE_CURRENT_BINARY_DIR}/${IOS_TOOLCHAIN_HOST}-nm")


include(CMakeForceCompiler)

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Darwin")
set(IOS True)

# Define the compiler
CMAKE_FORCE_C_COMPILER(${IOS_TOOLCHAIN_CC} Clang)
CMAKE_FORCE_CXX_COMPILER(${IOS_TOOLCHAIN_CXX} Clang)

set(CMAKE_FIND_ROOT_PATH ${CMAKE_OSX_SYSROOT} ${CMAKE_INSTALL_PREFIX})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

