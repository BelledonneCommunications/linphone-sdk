################################################################################
#
#  Copyright (c) 2021 Belledonne Communications SARL.
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
################################################################################

include(LinphoneSdkPlatformCommon)
include(LinphoneSdkCheckBuildToolsDesktop)

#set(LINPHONESDK_MACOS_ARCHS "x86_64" CACHE STRING "MacOS architectures to build for: comma-separated list of values in [x86_64]")
#set(LINPHONESDK_MACOS_ARCHS "arm64" CACHE STRING "MacOS architectures to build for: comma-separated list of values in [arm64]")
if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64")
    set(LINPHONESDK_MACOS_ARCHS "arm64, x86_64" CACHE STRING "MacOS architectures to build for: comma-separated list of values in [arm64, x86_64]")
else()
    set(LINPHONESDK_MACOS_ARCHS "x86_64" CACHE STRING "MacOS architectures to build for: comma-separated list of values in [x86_64]")
endif()
message(STATUS "CMAKE_HOST_SYSTEM_PROCESSOR is ${CMAKE_HOST_SYSTEM_PROCESSOR}. Selected architectures are [${LINPHONESDK_MACOS_ARCHS}]")
message(STATUS "CMAKE_APPLE_SILICON_PROCESSOR : ${CMAKE_APPLE_SILICON_PROCESSOR}")
linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_MACOS_ARCHS}" _archs)
list(GET _archs 0 _first_arch)

if(LINPHONESDK_PREBUILD_DEPENDENCIES)
        set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()
set(_macos_build_targets)

linphone_sdk_get_inherited_cmake_args()
linphone_sdk_get_enable_cmake_args()
linphone_sdk_get_sdk_cmake_args()
list(APPEND _cmake_args ${_enable_cmake_args})
list(APPEND _cmake_args ${_linphone_sdk_cmake_vars})

set(LINPHONESDK_MACOS_BASE_URL "https://www.linphone.org/releases/macosx/sdk" CACHE STRING "URL of the repository where the macos SDK zip files are located")
foreach(_arch IN LISTS _archs)
	set(_macos_install_prefix "${CMAKE_BINARY_DIR}/linphone-sdk/mac-${_arch}")
	set(_macos_prefix_path ${_macos_install_prefix} ${CMAKE_PREFIX_PATH})
	string(REPLACE ";" "|" _macos_prefix_path "${_macos_prefix_path}")
	set(_cmake_args
	"-DCMAKE_INSTALL_PREFIX=${_macos_install_prefix}"
	"-DCMAKE_PREFIX_PATH=${_macos_prefix_path}"
	"-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
	"-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/mac-${_arch}"
	"-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${CMAKE_SOURCE_DIR}"
	"-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-desktop.cmake"
	"-DCMAKE_CROSSCOMPILING=YES"
	"-DCMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD=TRUE"
	"-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
	"-DCMAKE_TOOLCHAIN_FILE=toolchains/toolchain-macos-${_arch}.cmake"
	)

#To remove if toolchain is good enough
#	list(APPEND _cmake_args "-DCMAKE_SYSTEM_PROCESSOR=${_arch}")
	set(SYSTEM_GENERATOR "${CMAKE_GENERATOR}")

	
	list(APPEND _cmake_args ${_enable_cmake_args} "-DCMAKE_GENERATOR=${SYSTEM_GENERATOR}")
	
	list(APPEND _cmake_args ${_linphone_sdk_cmake_vars})
	#We have to remove the defined CMAKE_INSTALL_PREFIX from inherited variables.
	#Because cache variables take precedence and we redefine it here for multi-arch
	ExcludeFromList(_cmake_cache_args CMAKE_INSTALL_PREFIX ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args CMAKE_SYSTEM_NAME ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args CMAKE_PREFIX_PATH ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args CMAKE_NO_SYSTEM_FROM_IMPORTED ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args LINPHONE_BUILDER_WORK_DIR ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args LINPHONE_BUILDER_EXTERNAL_SOURCE_PATH ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args CMAKE_CROSSCOMPILING ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args CMAKE_SYSTEM_VERSION ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args CMAKE_SYSTEM_PROCESSOR ${_inherited_cmake_args})
	ExcludeFromList(_cmake_cache_args CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ${_inherited_cmake_args})
	message("${_arch} : ${_cmake_args}, ${SYSTEM_GENERATOR}")
	ExternalProject_Add(mac-${_arch}
		${_ep_depends}
		SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake-builder"
		BINARY_DIR "${CMAKE_BINARY_DIR}/mac-${_arch}"
		CMAKE_GENERATOR "${SYSTEM_GENERATOR}"
		CMAKE_ARGS ${_cmake_args}
		#CMAKE_CACHE_ARGS ${_cmake_cache_args}
		INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
		LIST_SEPARATOR | # Use the alternate list separator
	)
	ExternalProject_Add_Step(mac-${_arch} force_build
		COMMENT "Forcing build for 'mac-${_arch}'"
		DEPENDEES configure
		DEPENDERS build
		ALWAYS 1
	)
	list(APPEND _macos_build_targets mac-${_arch})
endforeach()



add_custom_target(lipo ALL
	"${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_MACOS_ARCHS=${LINPHONESDK_MACOS_ARCHS}" "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-P" "${LINPHONESDK_DIR}/cmake/macos/Lipo.cmake"
	COMMENT "Aggregating frameworks of all architectures using lipo"
	DEPENDS ${_macos_build_targets}
)

add_custom_target(sdk ALL
	"${CMAKE_COMMAND}" "-DLINPHONESDK_PLATFORM=${LINPHONESDK_PLATFORM}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}" "-DLINPHONESDK_ENABLED_FEATURES_FILENAME=${CMAKE_BINARY_DIR}/enabled_features.txt" "-DLINPHONESDK_MACOS_ARCHS=${LINPHONESDK_MACOS_ARCHS}" "-DLINPHONESDK_MACOS_BASE_URL=${LINPHONESDK_MACOS_BASE_URL}" "-DLINPHONE_APP_EXT_FRAMEWORKS=${LINPHONE_APP_EXT_FRAMEWORKS}" "-DLINPHONE_OTHER_FRAMEWORKS=${LINPHONE_OTHER_FRAMEWORKS}" "-P" "${LINPHONESDK_DIR}/cmake/macos/GenerateSDK.cmake"
	COMMENT "Generating the SDK (zip file and podspec)"
	DEPENDS lipo
)
