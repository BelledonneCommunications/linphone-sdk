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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

set(CMAKE_ANDROID_API 14)
set(CMAKE_ANDROID_STL_TYPE gnustl_shared)

set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE YES)

# Define name of the target system
set(CMAKE_SYSTEM_NAME "Linux")
set(ANDROID True)

find_program(ANDROID_NDK_BUILD_PROGRAM ndk-build)
if(NOT ANDROID_NDK_BUILD_PROGRAM)
	message(FATAL_ERROR "Cannot find 'ndk-build', make sure you installed the NDK and added it to your PATH")
endif()
get_filename_component(ANDROID_NDK_PATH "${ANDROID_NDK_BUILD_PROGRAM}" DIRECTORY)

find_program(ANDROID_ANDROID_PROGRAM android)
if(NOT ANDROID_ANDROID_PROGRAM)
	message(FATAL_ERROR "Cannot find 'android', make sure you installed the SDK and added it to your PATH")
endif()
get_filename_component(ANDROID_SDK_PATH "${ANDROID_ANDROID_PROGRAM}" DIRECTORY)

find_program(ANDROID_ADB_PROGRAM adb)
if(NOT ANDROID_ADB_PROGRAM)
	message(FATAL_ERROR "Cannot find 'adb', make sure you installed the SDK platform tools and added it to your PATH")
endif()
get_filename_component(ANDROID_SDK_PLATFORM_TOOLS_PATH "${ANDROID_ADB_PROGRAM}" DIRECTORY)

find_file(CLANG_EXECUTABLE "clang"
	PATHS
	"${ANDROID_NDK_PATH}/toolchains/llvm/prebuilt/linux-x86_64/bin"
	"${ANDROID_NDK_PATH}/toolchains/llvm/prebuilt/darwin-x86_64/bin"
	"${ANDROID_NDK_PATH}/toolchains/llvm/prebuilt/linux-x86/bin"
	NO_DEFAULT_PATH
)
if(CLANG_EXECUTABLE)

	get_filename_component(TOOLCHAIN_PATH "${CLANG_EXECUTABLE}" DIRECTORY)
	set(GCC_VERSION "4.9")
	if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
		set(GCC_LIBRARY_ADDITIONAL_DIR "/armv7-a")
	endif()

	find_file(GCC_LIBRARY "libgcc.a"
		PATHS
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}${GCC_LIBRARY_ADDITIONAL_DIR}"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}.x${GCC_LIBRARY_ADDITIONAL_DIR}"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}.x"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/darwin-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}${GCC_LIBRARY_ADDITIONAL_DIR}"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/darwin-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/darwin-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}.x${GCC_LIBRARY_ADDITIONAL_DIR}"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/darwin-x86_64/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}.x"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}${GCC_LIBRARY_ADDITIONAL_DIR}"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}.x${GCC_LIBRARY_ADDITIONAL_DIR}"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86/lib/gcc/${COMPILER_PREFIX}/${GCC_VERSION}.x"
		NO_DEFAULT_PATH
	)
	if(NOT GCC_LIBRARY)
		message(FATAL_ERROR "Cannot find libgcc.a")
	endif()
	get_filename_component(GCC_LIBRARY_PATH "${GCC_LIBRARY}" DIRECTORY)

	find_file(GCC_EXECUTABLE "${COMPILER_PREFIX}-gcc"
		PATHS
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/darwin-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/darwin-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86/bin"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86/bin"
		NO_DEFAULT_PATH
	)
	if(NOT GCC_EXECUTABLE)
		message(FATAL_ERROR "Cannot find gcc")
	endif()
	get_filename_component(EXTERNAL_TOOLCHAIN_PATH "${GCC_EXECUTABLE}" DIRECTORY)

	#Define the compiler
	set(_CMAKE_TOOLCHAIN_PREFIX "${COMPILER_PREFIX}-")
	set(CMAKE_C_COMPILER "${CLANG_EXECUTABLE}")
	set(CMAKE_C_COMPILER_TARGET "${CLANG_TARGET}")
	set(CMAKE_C_COMPILER_EXTERNAL_TOOLCHAIN "${EXTERNAL_TOOLCHAIN_PATH}/..")
	set(CMAKE_CXX_COMPILER "${CLANG_EXECUTABLE}++")
	set(CMAKE_CXX_COMPILER_TARGET "${CLANG_TARGET}")
	set(CMAKE_CXX_COMPILER_EXTERNAL_TOOLCHAIN "${EXTERNAL_TOOLCHAIN_PATH}/..")

	set(TOOLCHAIN_PREFIX "${EXTERNAL_TOOLCHAIN_PATH}/${COMPILER_PREFIX}-")
	set(TOOLCHAIN_STRIP "${TOOLCHAIN_PREFIX}strip")

else()

	set(GCC_VERSION "4.8")

	find_file(GCC_EXECUTABLE "${COMPILER_PREFIX}-gcc"
		PATHS
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/darwin-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/darwin-x86_64/bin"
		"${ANDROID_NDK_PATH}/toolchains/${COMPILER_PREFIX}-${GCC_VERSION}/prebuilt/linux-x86/bin"
		"${ANDROID_NDK_PATH}/toolchains/${CMAKE_SYSTEM_PROCESSOR}-${GCC_VERSION}/prebuilt/linux-x86/bin"
		NO_DEFAULT_PATH
	)
	if(NOT GCC_EXECUTABLE)
		message(FATAL_ERROR "Cannot find the compiler")
	endif()
	get_filename_component(TOOLCHAIN_PATH "${GCC_EXECUTABLE}" DIRECTORY)

	set(TOOLCHAIN_PREFIX "${TOOLCHAIN_PATH}/${COMPILER_PREFIX}-")
	set(TOOLCHAIN_CC "${TOOLCHAIN_PREFIX}gcc")
	set(TOOLCHAIN_CXX "${TOOLCHAIN_PREFIX}g++")
	set(TOOLCHAIN_LD "${TOOLCHAIN_PREFIX}ld")
	set(TOOLCHAIN_AR "${TOOLCHAIN_PREFIX}ar")
	set(TOOLCHAIN_RANLIB "${TOOLCHAIN_PREFIX}ranlib")
	set(TOOLCHAIN_STRIP "${TOOLCHAIN_PREFIX}strip")
	set(TOOLCHAIN_NM "${TOOLCHAIN_PREFIX}nm")

	# Define the compiler
	set(CMAKE_C_COMPILER "${TOOLCHAIN_CC}")
	set(CMAKE_CXX_COMPILER "${TOOLCHAIN_CXX}")

endif()

set(CMAKE_SYSROOT "${ANDROID_NDK_PATH}/platforms/android-${CMAKE_ANDROID_API}/arch-${ARCHITECTURE}")
set(ANDROID_SUPPORT "${ANDROID_NDK_PATH}/sources/android/support")
set(ANDROID_SUPPORT_INCLUDE_DIRS "${ANDROID_SUPPORT}/include")

message(STATUS "Using sysroot path: ${CMAKE_SYSROOT}")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${CMAKE_INSTALL_PREFIX}" "${ANDROID_SUPPORT}")

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ANDROID_STL_INCLUDE_DIRS "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/${GCC_VERSION}/include" "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/${GCC_VERSION}/libs/${CMAKE_SYSTEM_PROCESSOR}/include")
if(EXISTS "${ANDROID_NDK_PATH}/sources/android/cpufeatures/cpu-features.c")
	set(ANDROID_CPU_FEATURES_INCLUDE_DIRS "${ANDROID_NDK_PATH}/sources/android/cpufeatures")
elseif(EXISTS "${ANDROID_NDK_PATH}/sources/cpufeatures/cpu-features.c")
	set(ANDROID_CPU_FEATURES_INCLUDE_DIRS "${ANDROID_NDK_PATH}/sources/cpufeatures")
else()
	message(FATAL_ERROR "Cannot find cpu-features.c")
endif()
include_directories(SYSTEM ${ANDROID_CPU_FEATURES_INCLUDE_DIRS} ${ANDROID_STL_INCLUDE_DIRS} ${ANDROID_SUPPORT_INCLUDE_DIRS})
add_definitions("-DANDROID")
if(GCC_LIBRARY_PATH)
	#link_directories("${GCC_LIBRARY_PATH}")
	# link_directories has no effet for external projects so add the GCC library path to the compiler flags
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${GCC_LIBRARY_PATH}" CACHE STRING "linker flags" FORCE)
	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L${GCC_LIBRARY_PATH}" CACHE STRING "linker flags" FORCE)
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${GCC_LIBRARY_PATH}" CACHE STRING "linker flags" FORCE)
endif()
set(GNUSTL_PATH "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/${GCC_VERSION}/libs/${CMAKE_SYSTEM_PROCESSOR}")
#link_directories("${GNUSTL_PATH}")
# link_directories has no effet for external projects so add the gnustl library path to the compiler flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${GNUSTL_PATH}" CACHE STRING "linker flags" FORCE)
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L${GNUSTL_PATH}" CACHE STRING "linker flags" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${GNUSTL_PATH}" CACHE STRING "linker flags" FORCE)

set(CMAKE_CXX_FLAGS_RELEASE             "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE               "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO      "-Os -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO        "-Os -g -DNDEBUG")

link_libraries("${GNUSTL_PATH}/libgnustl_shared.so")
link_libraries("log")
link_libraries("atomic")
