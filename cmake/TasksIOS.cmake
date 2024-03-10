############################################################################
# TasksIOS.cmake
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


set(LINPHONESDK_IOS_ARCHS "arm64, x86_64" CACHE STRING "iOS architectures to build: comma-separated list of values in [arm64, x86_64]")
set(LINPHONESDK_IOS_PLATFORM "Both" CACHE STRING "Platform to build for iOS. Value can be either \"Iphone\", \"Simulator\" or \"Both\"")
set(LINPHONESDK_IOS_BASE_URL "https://www.linphone.org/releases/ios" CACHE STRING "URL of the repository where the iOS SDK zip files are located.")
set(LINPHONESDK_SWIFT_DOC_HOSTING_PATH "snapshots/docs/liblinphone/${LINPHONESDK_VERSION}/swift" CACHE STRING "Hosting path of the swift doc.")
option(ENABLE_FAT_BINARY "Enable fat binary generation using lipo." ON)


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_IOS_ARCHS}" _IOS_ARCHS)


if(LINPHONESDK_IOS_PLATFORM STREQUAL "Iphone")
	list(REMOVE_ITEM _IOS_ARCHS "x86_64")
else()
	list(FIND _IOS_ARCHS "arm64" _FOUND)
	if(_FOUND GREATER -1)
		list(APPEND _IOS_ARCHS "arm64-simulator")
		if(LINPHONESDK_IOS_PLATFORM STREQUAL "Simulator")
			list(REMOVE_ITEM _IOS_ARCHS "arm64")
		endif()
	endif()
	list(FIND _IOS_ARCHS "x86_64" _FOUND)
	if(_FOUND GREATER -1)
		list(APPEND _IOS_ARCHS "x86_64-simulator")
		list(REMOVE_ITEM _IOS_ARCHS "x86_64")
	endif()
endif()
list(LENGTH _IOS_ARCHS _NB_IOS_ARCHS)
if(_NB_IOS_ARCHS LESS 1)
	message(FATAL_ERROR "Wrong combination of LINPHONESDK_IOS_ARCHS and LINPHONESDK_IOS_PLATFORM that results in no architecture being built!")
endif()


if(ENABLE_FAT_BINARY)
	set(LINPHONE_APP_EXT_FRAMEWORKS
		"bctoolbox.framework,belcard.framework,belle-sip.framework,belr.framework,lime.framework,linphone.framework,mediastreamer2.framework,msamr.framework,mscodec2.framework,msopenh264.framework,mssilk.framework,mswebrtc.framework,ortp.framework"
		CACHE STRING "Frameworks which are safe for app extension use"
  )
	set(LINPHONE_OTHER_FRAMEWORKS "bctoolbox-ios.framework" CACHE STRING "Frameworks which aren't safe for app extension use")
else()
	set(LINPHONE_APP_EXT_FRAMEWORKS
		"bctoolbox.xcframework,belcard.xcframework,belle-sip.xcframework,belr.xcframework,lime.xcframework,linphone.xcframework,mediastreamer2.xcframework,msamr.xcframework,mscodec2.xcframework,msopenh264.xcframework,mssilk.xcframework,mswebrtc.xcframework,ortp.xcframework"
		CACHE STRING "XCFrameworks which are safe for app extension use"
	)
	set(LINPHONE_OTHER_FRAMEWORKS "bctoolbox-ios.xcframework" CACHE STRING "XCFrameworks which aren't safe for app extension use")
endif()

if(ENABLE_VIDEO)
	set(LINPHONESDK_NAME "linphone-sdk")
else()
	set(LINPHONESDK_NAME "linphone-sdk-novideo")
endif()

set(_IOS_INSTALL_DIR "${PROJECT_BINARY_DIR}/${LINPHONESDK_NAME}")

############################################################################
# Clean previously generated SDK
############################################################################

add_custom_target(clean-sdk ALL
	COMMAND "${CMAKE_COMMAND}"
		"-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
		"-DLINPHONESDK_BUILD_DIR=${PROJECT_BINARY_DIR}"
		"-DCMAKE_INSTALL_PREFIX=${_IOS_INSTALL_DIR}"
		"-P" "${PROJECT_SOURCE_DIR}/cmake/IOS/CleanSDK.cmake"
	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Cleaning SDK"
	USES_TERMINAL
)


############################################################################
# Build each selected architecture
############################################################################

linphone_sdk_get_inherited_cmake_args(_CMAKE_CONFIGURE_ARGS _CMAKE_BUILD_ARGS)
linphone_sdk_get_enable_cmake_args(_IOS_CMAKE_ARGS)

set(_IOS_TARGETS)
foreach(_IOS_ARCH IN LISTS _IOS_ARCHS)
	if(_IOS_ARCH STREQUAL "arm64-simulator")
		list(APPEND _IOS_CMAKE_ARGS "-DENABLE_VPX=OFF") # libvpx build system does not support build for arm64 simulator
	endif()

	set(_IOS_ARCH_BINARY_DIR "${PROJECT_BINARY_DIR}/ios-${_IOS_ARCH}")
	set(_IOS_ARCH_INSTALL_DIR "${_IOS_INSTALL_DIR}/ios-${_IOS_ARCH}")
	add_custom_target(ios-${_IOS_ARCH} ALL
		COMMAND env -i "PATH=$ENV{PATH}" ${CMAKE_COMMAND} --preset=ios-${_IOS_ARCH} -B ${_IOS_ARCH_BINARY_DIR} ${_CMAKE_CONFIGURE_ARGS} ${_IOS_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${_IOS_ARCH_INSTALL_DIR}
		COMMAND ${CMAKE_COMMAND} --build ${_IOS_ARCH_BINARY_DIR} --target install ${_CMAKE_BUILD_ARGS}
		DEPENDS clean-sdk
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
		COMMENT "Building Linphone SDK for iOS ${_IOS_ARCH}"
		USES_TERMINAL
		COMMAND_EXPAND_LISTS
	)
	list(APPEND _IOS_TARGETS ios-${_IOS_ARCH})
endforeach()


############################################################################
# Generate the aggregated frameworks
############################################################################
message("preparing to GenerateFrameworks")
list(JOIN _IOS_ARCHS ", " _IOS_ARCHS)

set(CONFIGURATION "RelWithDebInfo")
if (CMAKE_BUILD_TYPE)
	set(CONFIGURATION "${CMAKE_BUILD_TYPE}")
elseif (CMAKE_CONFIGURATION_TYPES)
	if (NOT "RelWithDebInfo" IN_LIST CMAKE_CONFIGURATION_TYPES)
		list(GET CMAKE_CONFIGURATION_TYPES 0 CONFIGURATION)
	endif()
endif()

add_custom_target(gen-frameworks ALL
	COMMAND "${CMAKE_COMMAND}"
		"-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
		"-DLINPHONESDK_BUILD_DIR=${PROJECT_BINARY_DIR}"
		"-DLINPHONESDK_NAME=${LINPHONESDK_NAME}"
		"-DLINPHONESDK_IOS_ARCHS=${_IOS_ARCHS}"
		"-DLINPHONESDK_IOS_PLATFORM=${LINPHONESDK_IOS_PLATFORM}"
		"-DLINPHONESDK_SWIFT_DOC_HOSTING_PATH=${LINPHONESDK_SWIFT_DOC_HOSTING_PATH}"
		"-DCMAKE_INSTALL_PREFIX=${_IOS_INSTALL_DIR}/apple-darwin"
		"-DENABLE_FAT_BINARY=${ENABLE_FAT_BINARY}"
		"-DENABLE_SWIFT_DOC=${ENABLE_SWIFT_DOC}"
		"-DENABLE_SWIFT_WRAPPER=${ENABLE_SWIFT_WRAPPER}"
		"-DENABLE_SWIFT_WRAPPER_COMPILATION=${ENABLE_SWIFT_WRAPPER_COMPILATION}"
		"-DCONFIGURATION=${CONFIGURATION}"
		"-P" "${PROJECT_SOURCE_DIR}/cmake/IOS/GenerateFrameworks.cmake"
	DEPENDS ${_IOS_TARGETS}
	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Aggregating frameworks of all architectures"
	USES_TERMINAL
)


############################################################################
# Generate the SDK
############################################################################

list(GET _MACOS_ARCHS 0 _FIRST_ARCH)
add_custom_target(sdk ALL
	COMMAND "${CMAKE_COMMAND}"
		"-DLINPHONESDK_PLATFORM=${LINPHONESDK_PLATFORM}"
		"-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
		"-DLINPHONESDK_BUILD_DIR=${PROJECT_BINARY_DIR}"
		"-DLINPHONESDK_NAME=${LINPHONESDK_NAME}"
		"-DLINPHONESDK_STATE=${LINPHONESDK_STATE}"
		"-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
		"-DLINPHONESDK_ENABLED_FEATURES_FILENAME=${CMAKE_BINARY_DIR}/enabled_features.txt"
		"-DLINPHONESDK_IOS_ARCHS=${LINPHONESDK_IOS_ARCHS}"
		"-DLINPHONESDK_IOS_BASE_URL=${LINPHONESDK_IOS_BASE_URL}"
		"-DCMAKE_INSTALL_PREFIX=${_IOS_INSTALL_DIR}/apple-darwin"
		"-DLINPHONE_APP_EXT_FRAMEWORKS=${LINPHONE_APP_EXT_FRAMEWORKS}"
		"-DLINPHONE_OTHER_FRAMEWORKS=${LINPHONE_OTHER_FRAMEWORKS}"
		"-DENABLE_FAT_BINARY=${ENABLE_FAT_BINARY}"
		"-P" "${PROJECT_SOURCE_DIR}/cmake/IOS/GenerateSDK.cmake"
	DEPENDS gen-frameworks
	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Generating the SDK (zip file and podspec)"
	USES_TERMINAL
)
