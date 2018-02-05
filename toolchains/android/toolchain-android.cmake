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

set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
if(NOT CMAKE_ANDROID_NDK)
	find_program(ANDROID_NDK_BUILD_PROGRAM ndk-build)
	if(NOT ANDROID_NDK_BUILD_PROGRAM)
		message(FATAL_ERROR "Cannot find 'ndk-build', make sure you installed the NDK and added it to your PATH")
	endif()
	get_filename_component(CMAKE_ANDROID_NDK "${ANDROID_NDK_BUILD_PROGRAM}" DIRECTORY)
endif()

file(READ "${CMAKE_ANDROID_NDK}/source.properties" SOURCE_PROPERTIES_CONTENT)
string(REGEX MATCH "Pkg\\.Revision = ([0-9]+)\\." NDK_VERSION_MATCH "${SOURCE_PROPERTIES_CONTENT}")
set(CMAKE_ANDROID_NDK_VERSION ${CMAKE_MATCH_1})

set(CMAKE_SYSTEM_NAME "Android")
if(NOT CMAKE_ANDROID_API)
	set(CMAKE_ANDROID_API 16)
endif()

if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 16)
	set(CMAKE_ANDROID_STL_TYPE "gnustl_shared")
else()
	set(CMAKE_ANDROID_STL_TYPE "c++_shared")
endif()

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${CMAKE_INSTALL_PREFIX}")
# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
