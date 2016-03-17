############################################################################
# toolchain-android.cmake
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

set(CMAKE_ANDROID_API 14)
set(CMAKE_ANDROID_STL_TYPE gnustl_static)

find_path(ANDROID_NDK_PATH ndk-build)
if(NOT ANDROID_NDK_PATH)
	message(FATAL_ERROR "Cannot find 'ndk-build', make sure you installed the NDK and added it to your PATH")
endif()

find_path(ANDROID_SDK_PATH android)
if(NOT ANDROID_SDK_PATH)
	message(FATAL_ERROR "Cannot find 'android', make sure you installed the SDK and added it to your PATH")
endif()

find_path(ANDROID_SDK_PLATFORM_TOOLS_PATH adb)
if(NOT ANDROID_SDK_PLATFORM_TOOLS_PATH)
	message(FATAL_ERROR "Cannot find 'adb', make sure you installed the SDK platform tools and added it to your PATH")
endif()

find_file(GCC_EXECUTABLE
	NAMES
	"${COMPILER_PREFIX}-gcc"
	PATHS
	"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-4.8/prebuilt/linux-x86_64/bin"
	"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-4.8/prebuilt/linux-x86_64/bin"
	"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-4.8/prebuilt/darwin-x86_64/bin"
	"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-4.8/prebuilt/darwin-x86_64/bin"
	"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-4.8/prebuilt/linux-x86/bin"
	"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-4.8/prebuilt/linux-x86/bin"
)
if(NOT GCC_EXECUTABLE)
	message(FATAL_ERROR "Cannot find the compiler")
endif()
get_filename_component(ANDROID_TOOLCHAIN_PATH "${GCC_EXECUTABLE}" DIRECTORY)

set(CMAKE_SYSROOT "${ANDROID_NDK_PATH}/platforms/android-${CMAKE_ANDROID_API}/arch-${ARCHITECTURE}")

message(STATUS "Using sysroot path: ${CMAKE_SYSROOT}")

set(ANDROID_TOOLCHAIN_PREFIX "${ANDROID_TOOLCHAIN_PATH}/${COMPILER_PREFIX}-")
set(ANDROID_TOOLCHAIN_CC "${ANDROID_TOOLCHAIN_PREFIX}gcc")
set(ANDROID_TOOLCHAIN_CXX "${ANDROID_TOOLCHAIN_PREFIX}g++")
set(ANDROID_TOOLCHAIN_LD "${ANDROID_TOOLCHAIN_PREFIX}ld")
set(ANDROID_TOOLCHAIN_AR "${ANDROID_TOOLCHAIN_PREFIX}ar")
set(ANDROID_TOOLCHAIN_RANLIB "${ANDROID_TOOLCHAIN_PREFIX}ranlib")
set(ANDROID_TOOLCHAIN_STRIP "${ANDROID_TOOLCHAIN_PREFIX}strip")
set(ANDROID_TOOLCHAIN_NM "${ANDROID_TOOLCHAIN_PREFIX}nm")

include(CMakeForceCompiler)

set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE YES)

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Linux")
set(ANDROID True)

# Define the compiler
CMAKE_FORCE_C_COMPILER("${ANDROID_TOOLCHAIN_CC}" GNU)
CMAKE_FORCE_CXX_COMPILER("${ANDROID_TOOLCHAIN_CXX}" GNU)

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${CMAKE_INSTALL_PREFIX}")

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ANDROID_STL_INCLUDE_DIRS "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.8/include" "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.8/libs/${CMAKE_SYSTEM_PROCESSOR}/include")
set(ANDROID_MACHINE_INCLUDE_DIRS "${ANDROID_NDK_PATH}/sources/cpufeatures")
include_directories(SYSTEM ${ANDROID_MACHINE_INCLUDE_DIRS} ${ANDROID_STL_INCLUDE_DIRS})
link_libraries("${ANDROID_NDK_PATH}/sources/android/libportable/libs/${CMAKE_SYSTEM_PROCESSOR}/libportable.a")
link_libraries("${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/4.8/libs/${CMAKE_SYSTEM_PROCESSOR}/libgnustl_static.a")
link_libraries("log")
