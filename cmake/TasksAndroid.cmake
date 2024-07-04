############################################################################
# TasksAndroid.cmake
# Copyright (C) 2010-2024 Belledonne Communications, Grenoble France
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

include(ExternalProject)


set(LINPHONESDK_ANDROID_ARCHS "arm64, armv7" CACHE STRING "Android architectures to build: comma-separated list of values in [arm64, armv7, x86, x86_64]")


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_ANDROID_ARCHS}" _ANDROID_ARCHS)


############################################################################
# Run gradle clean and generate gradle build scripts
############################################################################

list(GET _ANDROID_ARCHS 0 _FIRST_ARCH)
add_custom_target(gradle-clean ALL
	COMMAND "${CMAKE_COMMAND}"
    "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
    "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
    "-DLINPHONESDK_STATE=${LINPHONESDK_STATE}"
    "-DLINPHONESDK_BRANCH=${LINPHONESDK_BRANCH}"
    "-DLINPHONESDK_FIRST_ARCH=${_FIRST_ARCH}"
    "-DENABLE_VIDEO=${ENABLE_VIDEO}"
    "-P" "${PROJECT_SOURCE_DIR}/cmake/Android/GradleClean.cmake"
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  COMMENT "Generating gradle build scripts and performing gradle clean"
  USES_TERMINAL
)


############################################################################
# Build each selected architecture
############################################################################

linphone_sdk_get_inherited_cmake_args(_CMAKE_CONFIGURE_ARGS _CMAKE_BUILD_ARGS)
linphone_sdk_get_enable_cmake_args(_ANDROID_CMAKE_ARGS)

set(_ANDROID_TARGETS)
foreach(_ANDROID_ARCH IN LISTS _ANDROID_ARCHS)
	set(_ANDROID_ARCH_BINARY_DIR "${PROJECT_BINARY_DIR}/android-${_ANDROID_ARCH}")
	set(_ANDROID_ARCH_INSTALL_DIR "${PROJECT_BINARY_DIR}/linphone-sdk/android-${_ANDROID_ARCH}")
  add_custom_target(android-${_ANDROID_ARCH} ALL
    COMMAND ${CMAKE_COMMAND} --preset=android-${_ANDROID_ARCH} -B ${_ANDROID_ARCH_BINARY_DIR} ${_CMAKE_CONFIGURE_ARGS} ${_ANDROID_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${_ANDROID_ARCH_INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${_ANDROID_ARCH_BINARY_DIR} --target install ${_CMAKE_BUILD_ARGS}
    DEPENDS gradle-clean
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Building Linphone SDK for Android ${_ANDROID_ARCH}"
    USES_TERMINAL
    COMMAND_EXPAND_LISTS
  )
  list(APPEND _ANDROID_TARGETS android-${_ANDROID_ARCH})
endforeach()


############################################################################
# Generate the SDK
############################################################################

add_custom_target(copy-libs ALL
	COMMAND "${CMAKE_COMMAND}"
    "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
    "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "-DLINPHONESDK_ANDROID_ARCHS=${LINPHONESDK_ANDROID_ARCHS}"
    "-DCMAKE_ANDROID_NDK=${CMAKE_ANDROID_NDK}"
    "-DIS_ASAN=$<CONFIG:ASAN>"
    "-DSTRIP_COMMAND=strip$<$<CONFIG:Debug>:-debug>.sh"
    "-P" "${PROJECT_SOURCE_DIR}/cmake/Android/CopyLibs.cmake"
	DEPENDS ${_ANDROID_TARGETS}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Copying libs"
  USES_TERMINAL
)

add_custom_target(sdk ALL
	COMMAND "${CMAKE_COMMAND}"
    "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
    "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "-DIS_DEBUG=$<CONFIG:ASAN,Debug>"
    "-DIS_RELEASE=$<CONFIG:Release>"
    "-P" "${PROJECT_SOURCE_DIR}/cmake/Android/GenerateSDK.cmake"
	DEPENDS copy-libs
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Generating the SDK (zip file and aar)"
  USES_TERMINAL
)
