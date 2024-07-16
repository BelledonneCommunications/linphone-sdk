############################################################################
# toolchain-android-common.cmake
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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
string(REGEX MATCH "Pkg\\.Revision = ([0-9]+)\\.([0-9]+)\\." NDK_VERSION_MATCH "${SOURCE_PROPERTIES_CONTENT}")
set(CMAKE_ANDROID_NDK_VERSION ${CMAKE_MATCH_1})
set(CMAKE_ANDROID_NDK_VERSION_MINOR ${CMAKE_MATCH_2})

if(NOT ANDROID_PLATFORM)
	if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 19)
		#set(ANDROID_NATIVE_API_LEVEL "android-17")
  	set(ANDROID_PLATFORM_LEVEL "android-17")
		set(ANDROID_PLATFORM "android-17")
	else()
		# Starting with NDK 19, API 17 no longer exists
		#set(ANDROID_NATIVE_API_LEVEL "android-21")
  	set(ANDROID_PLATFORM_LEVEL "android-21")
		set(ANDROID_PLATFORM "android-21")
	endif()
endif()

set(ANDROID_CPP_FEATURES "rtti exceptions")
set(ANDROID_STL "c++_shared")


# Add support for 16KiB page size.
if(CMAKE_ANDROID_NDK_VERSION EQUAL 27)
	set(ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES "ON")
endif()


#IF ANDROID_PLATFORM != the default one (official toolchain , cached values, or default values)
#: delete the -D__ANDROID_API__ and replace it in the cached CFLAGS flags


if(CMAKE_ANDROID_NDK_VERSION GREATER_EQUAL 23)
	include("${CMAKE_ANDROID_NDK}/build/cmake/android-legacy.toolchain.cmake")
else()
	include("${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake")
endif()

if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS 19)
	set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${ANDROID_SYSTEM_LIBRARY_PATH}" "${CMAKE_INSTALL_PREFIX}")
else()
	if(CMAKE_ANDROID_NDK_VERSION VERSION_EQUAL 19)
		# NDK 19b needs this, but not NDK 19
		if(CMAKE_ANDROID_NDK_VERSION_MINOR GREATER_EQUAL 1)
			set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${ANDROID_SYSTEM_LIBRARY_PATH}" "${CMAKE_INSTALL_PREFIX}")
		endif()
	else()
		set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}" "${ANDROID_SYSTEM_LIBRARY_PATH}" "${CMAKE_INSTALL_PREFIX}")
	endif()
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG")
