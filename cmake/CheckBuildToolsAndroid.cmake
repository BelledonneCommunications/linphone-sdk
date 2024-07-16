############################################################################
# CheckBuildToolsAndroid.cmake
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
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

if(NOT UNIX)
	message(FATAL_ERROR "You need to use a Linux or Mac OS X system to build for Android")
endif()


set(LINPHONESDK_ANDROID_MIN_SUPPORTED_NDK 20)
set(LINPHONESDK_ANDROID_MAX_SUPPORTED_NDK 27)

cmake_minimum_required(VERSION 3.22)


# Check Android NDK version
set(CMAKE_ANDROID_NDK "$ENV{ANDROID_NDK_HOME}")
if(NOT CMAKE_ANDROID_NDK)
	find_program(ANDROID_NDK_BUILD_PROGRAM "ndk-build")
	if(NOT ANDROID_NDK_BUILD_PROGRAM)
		message(FATAL_ERROR "Cannot find 'ndk-build', make sure you installed the NDK and added it to your PATH")
	endif()
	get_filename_component(CMAKE_ANDROID_NDK "${ANDROID_NDK_BUILD_PROGRAM}" DIRECTORY)
endif()
file(READ "${CMAKE_ANDROID_NDK}/source.properties" SOURCE_PROPERTIES_CONTENT)
string(REGEX MATCH "Pkg\\.Revision = ([0-9]+)\\." NDK_VERSION_MATCH "${SOURCE_PROPERTIES_CONTENT}")
set(CMAKE_ANDROID_NDK_VERSION ${CMAKE_MATCH_1})
if(CMAKE_ANDROID_NDK_VERSION VERSION_LESS ${LINPHONESDK_ANDROID_MIN_SUPPORTED_NDK} OR CMAKE_ANDROID_NDK_VERSION VERSION_GREATER ${LINPHONESDK_ANDROID_MAX_SUPPORTED_NDK})
	message(FATAL_ERROR "Unsupported Android NDK version ${CMAKE_ANDROID_NDK_VERSION}. Please install version ${LINPHONESDK_ANDROID_MAX_SUPPORTED_NDK}")
endif()


include("${CMAKE_CURRENT_LIST_DIR}/CheckBuildToolsCommon.cmake")
