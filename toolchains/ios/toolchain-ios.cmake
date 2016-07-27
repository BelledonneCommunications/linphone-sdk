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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
	OUTPUT_VARIABLE CLANG_PATH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT ${XCRUN_FIND_CLANG_RESULT} EQUAL 0)
	message(FATAL_ERROR "xcrun failed: ${XCRUN_FIND_CLANG_RESULT}. You may need to install Xcode.")
endif()
get_filename_component(TOOLCHAIN_PATH "${CLANG_PATH}" DIRECTORY)

message(STATUS "Using sysroot path: ${CMAKE_OSX_SYSROOT}")
message(STATUS "Using sdk version: ${IOS_SDK_VERSION}")
set(SDK_BIN_PATH "${CMAKE_OSX_SYSROOT}/../../usr/bin")

set(TOOLCHAIN_CC "${TOOLCHAIN_PATH}/clang")
set(TOOLCHAIN_CXX "${TOOLCHAIN_PATH}/clang++")
set(TOOLCHAIN_OBJC "${TOOLCHAIN_PATH}/clang")
set(TOOLCHAIN_LD "${TOOLCHAIN_PATH}/ld")
set(TOOLCHAIN_AR "${TOOLCHAIN_PATH}/ar")
set(TOOLCHAIN_RANLIB "${TOOLCHAIN_PATH}/ranlib")
set(TOOLCHAIN_STRIP "${TOOLCHAIN_PATH}/strip")
set(TOOLCHAIN_NM "${TOOLCHAIN_PATH}/nm")

execute_process(COMMAND xcodebuild -version OUTPUT_VARIABLE XCODE_VERSION_RAW OUTPUT_STRIP_TRAILING_WHITESPACE)
STRING(REGEX REPLACE "Xcode ([^\n]*).*" "\\1" XCODE_VERSION "${XCODE_VERSION_RAW}")

include(CMakeForceCompiler)

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Darwin")
set(IOS True)

# The following variables are needed to build correctly with Xcode
if(CMAKE_GENERATOR STREQUAL "Xcode")
	set(CMAKE_MACOSX_BUNDLE YES)
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO)
endif()

# Define the compiler
set(CMAKE_C_COMPILER "${TOOLCHAIN_CC}")
set(CMAKE_C_COMPILER_TARGET "${CLANG_TARGET}")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_CXX}")
set(CMAKE_CXX_COMPILER_TARGET "${CLANG_TARGET}")
set(CMAKE_AR "${TOOLCHAIN_AR}" CACHE FILEPATH "ar")
set(CMAKE_RANLIB "${TOOLCHAIN_RANLIB}" CACHE FILEPATH "ranlib")
set(CMAKE_LINKER "${TOOLCHAIN_LD}" CACHE FILEPATH "linker")
set(CMAKE_NM "${TOOLCHAIN_NM}" CACHE FILEPATH "nm")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_OSX_SYSROOT} ${CMAKE_INSTALL_PREFIX})
# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

#important: the GUI identifier is required so that executables can be launched on iOS devices/simulators
set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.linphone.\${PRODUCT_NAME:identifier}")
