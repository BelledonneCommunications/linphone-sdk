############################################################################
# LinphoneSdkPlatformDesktop.cmake
# Copyright (C) 2010-2018 Belledonne Communications, Grenoble France
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

include(LinphoneSdkPlatformCommon)
include(LinphoneSdkCheckBuildToolsDesktop)


if(APPLE)
	set(_dummy_libraries)
	if(NOT ENABLE_UNIT_TESTS)
		list(APPEND _dummy_libraries "linphonetester")
		endif()
	if(NOT ENABLE_AMRNB AND NOT ENABLE_AMRWB)
		list(APPEND _dummy_libraries "msamr")
	endif()
	if(NOT ENABLE_CODEC2)
		list(APPEND _dummy_libraries "mscodec2")
	endif()
	if(NOT ENABLE_OPENH264)
		list(APPEND _dummy_libraries "msopenh264")
	endif()
	if(NOT ENABLE_ILBC AND NOT ENABLE_ISAC AND NOT ENABLE_WEBRTC_AEC AND NOT ENABLE_WEBRTC_AECM AND NOT ENABLE_WEBRTC_VAD)
		list(APPEND _dummy_libraries "mswebrtc")
	endif()
endif()


set(_cmake_args
        "-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/linphone-sdk/desktop"
        "-DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/linphone-sdk/desktop"
	"-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
        "-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/desktop"
	"-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${PROJECT_SOURCE_DIR}"
	"-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-desktop.cmake"
)

if(_dummy_libraries)
	string(REPLACE ";" " " _dummy_libraries "${_dummy_libraries}")
	list(APPEND _cmake_args "-DLINPHONE_BUILDER_DUMMY_LIBRARIES=${_dummy_libraries}")
endif()

linphone_sdk_get_inherited_cmake_args()
linphone_sdk_get_enable_cmake_args()
list(APPEND _cmake_args ${_enable_cmake_args})

if(LINPHONESDK_PREBUILD_DEPENDENCIES)
	set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()

if(WIN32)
	set(_install_command "${PROJECT_SOURCE_DIR}/cmake/dummy.bat")
else()
	set(_install_command "${PROJECT_SOURCE_DIR}/cmake/dummy.sh")
endif()

ExternalProject_Add(sdk
	${_ep_depends}
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/cmake-builder"
        BINARY_DIR "${CMAKE_BINARY_DIR}/desktop"
	CMAKE_GENERATOR "${CMAKE_GENERATOR}"
	CMAKE_ARGS ${_cmake_args}
	CMAKE_CACHE_ARGS ${_inherited_cmake_args}
	INSTALL_COMMAND ${_install_command}
)



if(APPLE)
	add_custom_command(TARGET sdk
		COMMENT "Clean the SDK zip file"
		COMMAND "${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
		"-P" "${LINPHONESDK_DIR}/cmake/macos/CleanSDK.cmake"
	)
endif()

ExternalProject_Add_Step(sdk force_build
	COMMENT "Forcing build for 'desktop'"
	DEPENDEES configure
	DEPENDERS build
	ALWAYS 1
)

if(APPLE)
	set(LINPHONESDK_MACOS_BASE_URL "https://www.linphone.org/releases/macosx/" CACHE STRING "URL of the repository where the macos SDK zip files are located")
	add_custom_command(TARGET sdk
		COMMENT "Generating the SDK (zip file and podspec)"
		COMMAND "${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}" "-DLINPHONESDK_MACOS_BASE_URL=${LINPHONESDK_MACOS_BASE_URL}" "-DLINPHONESDK_ENABLED_FEATURES_FILENAME=${CMAKE_BINARY_DIR}/enabled_features.txt"
		"-P" "${LINPHONESDK_DIR}/cmake/macos/GenerateSDK.cmake"
	)
elseif(WIN32)
	set(LINPHONESDK_WINDOWS_BASE_URL "https://www.linphone.org/releases/windows/" CACHE STRING "URL of the repository where the Windows SDK zip files are located")
	add_custom_command(TARGET sdk
		COMMENT "Generating the SDK (zip file)"
		COMMAND "${CMAKE_COMMAND}" "-DLINPHONESDK_PLATEFORM=${CMAKE_GENERATOR_PLATFORM}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}" "-DLINPHONESDK_WINDOWS_BASE_URL=${LINPHONESDK_WINDOWS_BASE_URL}" "-DLINPHONESDK_ENABLED_FEATURES_FILENAME=${CMAKE_BINARY_DIR}/enabled_features.txt"
		"-P" "${LINPHONESDK_DIR}/cmake/Windows/GenerateSDK.cmake"
	)
endif()
