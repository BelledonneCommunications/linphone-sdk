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

set(ANDROID_NATIVE_API_LEVEL "android-16")
set(ANDROID_CPP_FEATURES "rtti exceptions")
set(ANDROID_STL "c++_shared")

include("${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake")

set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${ANDROID_SYSTEM_LIBRARY_PATH}" "${CMAKE_INSTALL_PREFIX}")

set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
