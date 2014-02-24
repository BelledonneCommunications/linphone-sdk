############################################################################
# toolchan-ios.cmake
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
endif(NOT APPLE)

set(SDK_VERSION 4.0)

execute_process(COMMAND xcode-select -print-path OUTPUT_VARIABLE XCODE_PATH OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT ${XCODE_SELECT_RESULT} EQUAL 0)
	message(FATAL_ERROR "xcode-select failed: ${XCODE_SELECT_RESULT}. You may need to install Xcode.")
endif(NOT ${XCODE_SELECT_RESULT} EQUAL 0)

if(EXISTS "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/")
	# New path with Xcode 4.3
	file(GLOB SDK_PATH_LIST "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/iPhone${PLATFORM}*.sdk")
	set(SDK_BIN_PATH "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/usr/bin")
else(EXISTS "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/")
	file(GLOB SDK_PATH_LIST "/Developer/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/iPhone${PLATFORM}*.sdk")
	set(SDK_BIN_PATH "/Developer/Platforms/iPhone${PLATFORM}.platform/Developer/usr/bin")
endif(EXISTS "${XCODE_PATH}/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/")
list(SORT SDK_PATH_LIST)
list(REVERSE SDK_PATH_LIST)
list(GET SDK_PATH_LIST 0 SYSROOT_PATH)
message(STATUS "Using sysroot path: ${SYSROOT_PATH}")

set(LINPHONE_BUILDER_TOOLCHAIN_HOST ${COMPILER_PREFIX})

foreach(TOOLNAME clang clang++ ld ar ranlib strip nm)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/toolchains/ios/tool_wrapper.sh.cmake ${CMAKE_CURRENT_BINARY_DIR}/${TOOLNAME}.sh)
endforeach(TOOLNAME)

set(LINPHONE_BUILDER_TOOLCHAIN_CC "${CMAKE_CURRENT_BINARY_DIR}/clang.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_CXX "${CMAKE_CURRENT_BINARY_DIR}/clang++.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_OBJC "${CMAKE_CURRENT_BINARY_DIR}/clang.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_LD "${CMAKE_CURRENT_BINARY_DIR}/ld.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_AR "${CMAKE_CURRENT_BINARY_DIR}/ar.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_RANLIB "${CMAKE_CURRENT_BINARY_DIR}/ranlib.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_STRIP "${CMAKE_CURRENT_BINARY_DIR}/strip.sh")
set(LINPHONE_BUILDER_TOOLCHAIN_NM "${CMAKE_CURRENT_BINARY_DIR}/nm.sh")

set(COMMON_FLAGS "-arch ${SYSTEM_PROCESSOR} -isysroot ${SYSROOT_PATH} -miphoneos-version-min=${SDK_VERSION} -DTARGET_OS_IPHONE=1 -D__IOS -fms-extensions")
set(LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS "${COMMON_FLAGS} -Dasm=__asm")
set(LINPHONE_BUILDER_TOOLCHAIN_CFLAGS "-std=c99")
set(LINPHONE_BUILDER_TOOLCHAIN_OBJCFLAGS "-std=c99 ${COMMON_FLAGS} -x objective-c -fexceptions -gdwarf-2 -fobjc-abi-version=2 -fobjc-legacy-dispatch")
set(LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS "-arch ${SYSTEM_PROCESSOR}")


include(CMakeForceCompiler)

set(CMAKE_CROSSCOMPILING TRUE)

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Darwin")
set(CMAKE_SYSTEM_PROCESSOR ${SYSTEM_PROCESSOR})

# Define the compiler
CMAKE_FORCE_C_COMPILER(${LINPHONE_BUILDER_TOOLCHAIN_CC} Clang)
CMAKE_FORCE_CXX_COMPILER(${LINPHONE_BUILDER_TOOLCHAIN_CXX} Clang)

set(CMAKE_FIND_ROOT_PATH ${SYSROOT_PATH} ${CMAKE_INSTALL_PREFIX})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Restrict pkg-config to search in the install directory
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

