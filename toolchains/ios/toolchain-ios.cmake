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

execute_process(COMMAND xcode-select -print-path
	RESULT_VARIABLE XCODE_SELECT_RESULT
	OUTPUT_VARIABLE XCODE_PATH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT ${XCODE_SELECT_RESULT} EQUAL 0)
	message(FATAL_ERROR "xcode-select failed: ${XCODE_SELECT_RESULT}. You may need to install Xcode.")
endif()

string(TOLOWER ${PLATFORM}  PLATFORM_LOWER)

execute_process(COMMAND xcrun --sdk iphone${PLATFORM_LOWER} --show-sdk-version
	RESULT_VARIABLE XCRUN_SHOW_SDK_VERSION_RESULT
	OUTPUT_VARIABLE IOS_SDK_VERSION
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT ${XCRUN_SHOW_SDK_VERSION_RESULT} EQUAL 0)
	message(FATAL_ERROR "xcrun failed: ${XCRUN_SHOW_SDK_VERSION_RESULT}. You may need to install Xcode.")
endif()

execute_process(COMMAND xcrun --sdk iphone${PLATFORM_LOWER} --show-sdk-path
	RESULT_VARIABLE XCRUN_SHOW_SDK_PATH_RESULT
	OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT ${XCRUN_SHOW_SDK_PATH_RESULT} EQUAL 0)
	message(FATAL_ERROR "xcrun failed: ${XCRUN_SHOW_SDK_PATH_RESULT}. You may need to install Xcode.")
endif()

execute_process(COMMAND xcrun --sdk iphone${PLATFORM_LOWER} --find clang
	RESULT_VARIABLE XCRUN_FIND_CLANG_RESULT
	OUTPUT_VARIABLE IOS_CLANG_PATH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT ${XCRUN_FIND_CLANG_RESULT} EQUAL 0)
	message(FATAL_ERROR "xcrun failed: ${XCRUN_FIND_CLANG_RESULT}. You may need to install Xcode.")
endif()
get_filename_component(IOS_TOOLCHAIN_PATH "${IOS_CLANG_PATH}" DIRECTORY)

message(STATUS "Using sysroot path: ${CMAKE_OSX_SYSROOT}")
message(STATUS "Using sdk version: ${IOS_SDK_VERSION}")
set(SDK_BIN_PATH "${CMAKE_OSX_SYSROOT}/../../usr/bin")

set(IOS_TOOLCHAIN_CC "${IOS_TOOLCHAIN_PATH}/clang")
set(IOS_TOOLCHAIN_CXX "${IOS_TOOLCHAIN_PATH}/clang++")
set(IOS_TOOLCHAIN_OBJC "${IOS_TOOLCHAIN_PATH}/clang")
set(IOS_TOOLCHAIN_LD "${IOS_TOOLCHAIN_PATH}/ld")
set(IOS_TOOLCHAIN_AR "${IOS_TOOLCHAIN_PATH}/ar")
set(IOS_TOOLCHAIN_RANLIB "${IOS_TOOLCHAIN_PATH}/ranlib")
set(IOS_TOOLCHAIN_STRIP "${IOS_TOOLCHAIN_PATH}/strip")
set(IOS_TOOLCHAIN_NM "${IOS_TOOLCHAIN_PATH}/nm")

execute_process(COMMAND xcodebuild -version OUTPUT_VARIABLE XCODE_VERSION_RAW OUTPUT_STRIP_TRAILING_WHITESPACE)
STRING(REGEX REPLACE "Xcode ([^\n]*).*" "\\1" XCODE_VERSION "${XCODE_VERSION_RAW}")

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
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
