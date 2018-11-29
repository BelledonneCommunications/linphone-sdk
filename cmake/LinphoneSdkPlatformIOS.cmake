############################################################################
# LinphoneSdkPlatformIOS.cmake
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
include(LinphoneSdkCheckBuildToolsIOS)


set(LINPHONESDK_IOS_ARCHS "arm64, armv7, x86_64" CACHE STRING "Android architectures to build for: comma-separated list of values in [arm64, armv7, x86_64]")
set(LINPHONESDK_IOS_BASE_URL "https://www.linphone.org/releases/ios/" CACHE STRING "URL of the repository where the iOS SDK zip files are located")


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
if(NOT ENABLE_SILK)
	list(APPEND _dummy_libraries "mssilk")
endif()
if(NOT ENABLE_ILBC AND NOT ENABLE_ISAC AND NOT ENABLE_WEBRTC_AEC AND NOT ENABLE_WEBRTC_AECM AND NOT ENABLE_WEBRTC_VAD)
	list(APPEND _dummy_libraries "mswebrtc")
endif()
if(NOT ENABLE_X264)
	list(APPEND _dummy_libraries "msx264")
endif()


if(LINPHONESDK_PREBUILD_DEPENDENCIES)
	set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()
set(_ios_build_targets)

linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_IOS_ARCHS}" _archs)
foreach(_arch IN LISTS _archs)
	set(_cmake_args
		"-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/linphone-sdk/${_arch}-apple-darwin.ios"
		"-DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/linphone-sdk/${_arch}-apple-darwin.ios"
		"-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
		"-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/ios-${_arch}"
		"-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${CMAKE_SOURCE_DIR}"
		"-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-ios-${_arch}.cmake"
		"-DCMAKE_TOOLCHAIN_FILE=toolchains/toolchain-ios-${_arch}.cmake"
	)
	if(_dummy_libraries)
		string(REPLACE ";" " " _dummy_libraries "${_dummy_libraries}")
		list(APPEND _cmake_args "-DLINPHONE_BUILDER_DUMMY_LIBRARIES=${_dummy_libraries}")
	endif()

	linphone_sdk_get_inherited_cmake_args()
	linphone_sdk_get_enable_cmake_args()
	list(APPEND _cmake_args ${_enable_cmake_args})

	ExternalProject_Add(ios-${_arch}
		${_ep_depends}
		SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake-builder"
		BINARY_DIR "${CMAKE_BINARY_DIR}/ios-${_arch}"
		CMAKE_GENERATOR "${CMAKE_GENERATOR}"
		CMAKE_ARGS ${_cmake_args}
		CMAKE_CACHE_ARGS ${_inherited_cmake_args}
		INSTALL_COMMAND "${CMAKE_SOURCE_DIR}/cmake/dummy.sh"
	)
	ExternalProject_Add_Step(ios-${_arch} force_build
		COMMENT "Forcing build for 'ios-${_arch}'"
		DEPENDEES configure
		DEPENDERS build
		ALWAYS 1
	)
	list(APPEND _ios_build_targets ios-${_arch})
endforeach()


add_custom_target(lipo ALL
	"${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_IOS_ARCHS=${LINPHONESDK_IOS_ARCHS}" "-P" "${LINPHONESDK_DIR}/cmake/IOS/Lipo.cmake"
	COMMENT "Aggregating frameworks of all architectures using lipo"
	DEPENDS ${_ios_build_targets}
)

add_custom_target(sdk
	"${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}" "-DLINPHONESDK_ENABLED_FEATURES_FILENAME=${CMAKE_BINARY_DIR}/enabled_features.txt" "-DLINPHONESDK_IOS_ARCHS=${LINPHONESDK_IOS_ARCHS}" "-DLINPHONESDK_IOS_BASE_URL=${LINPHONESDK_IOS_BASE_URL}" "-P" "${LINPHONESDK_DIR}/cmake/IOS/GenerateSDK.cmake"
	COMMENT "Generating the SDK (zip file and podspec)"
	DEPENDS lipo
)
