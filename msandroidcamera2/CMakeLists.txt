############################################################################
# CMakeLists.txt
# Copyright (C) 2019-2023 Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.22)

# Builds camera2 with android sdk 24 while other projects are built with another version
# (defined in linphone-sdk toolchain android file)
# This system ensures a retrocompatible build, and the activation of new features ONLY
# on devices supporting it
set(REQUIRED_NATIVE_API_LEVEL "24")
set(PREVIOUS_ANDROID_PREFIX_PATH )
foreach(_ITEM ${CMAKE_SYSTEM_PREFIX_PATH})
	if(_ITEM MATCHES ".*-android/${ANDROID_NATIVE_API_LEVEL}")
		set(PREVIOUS_ANDROID_PREFIX_PATH "${_ITEM}")
	endif()
endforeach()
list(REMOVE_ITEM CMAKE_SYSTEM_PREFIX_PATH ${PREVIOUS_ANDROID_PREFIX_PATH})
string(REPLACE "${ANDROID_NATIVE_API_LEVEL}" "${REQUIRED_NATIVE_API_LEVEL}" "REQUIRED_ANDROID_PREFIX_PATH" "${PREVIOUS_ANDROID_PREFIX_PATH}")
list(PREPEND CMAKE_SYSTEM_PREFIX_PATH "${REQUIRED_ANDROID_PREFIX_PATH}")
string(REPLACE "${ANDROID_NATIVE_API_LEVEL}" "${REQUIRED_NATIVE_API_LEVEL}" "CMAKE_C_COMPILER_TARGET" "${CMAKE_C_COMPILER_TARGET}")
string(REPLACE "${ANDROID_NATIVE_API_LEVEL}" "${REQUIRED_NATIVE_API_LEVEL}" "CMAKE_CXX_COMPILER_TARGET" "${CMAKE_CXX_COMPILER_TARGET}")
string(REPLACE "${ANDROID_NATIVE_API_LEVEL}" "${REQUIRED_NATIVE_API_LEVEL}" "CMAKE_ASM_COMPILER_TARGET" "${CMAKE_ASM_COMPILER_TARGET}")
unset(PREVIOUS_ANDROID_PREFIX_PATH)
unset(REQUIRED_ANDROID_PREFIX_PATH)
unset(REQUIRED_NATIVE_API_LEVEL)

project(MSAndroidCamera2 VERSION 0.1 LANGUAGES CXX)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/mediastreamer2/plugins")

include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

find_package(Mediastreamer2 5.3.0 REQUIRED)

set(SOURCE_FILES android-camera2-capture.cpp)
bc_apply_compile_flags(SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

if(BUILD_SHARED_LIBS)
	add_library(msandroidcamera2 MODULE ${SOURCE_FILES})
else()
	add_library(msandroidcamera2 STATIC ${SOURCE_FILES})
endif()
target_include_directories(msandroidcamera2 PRIVATE ${PROJECT_BINARY_DIR})
target_link_libraries(msandroidcamera2 PRIVATE ${Mediastreamer2_TARGET} android camera2ndk mediandk)


install(TARGETS msandroidcamera2
	RUNTIME DESTINATION ${Mediastreamer2_PLUGINS_DIR}
	LIBRARY DESTINATION ${Mediastreamer2_PLUGINS_DIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
