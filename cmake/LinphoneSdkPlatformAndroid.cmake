############################################################################
# LinphoneSdkPlatformAndroid.cmake
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


set(LINPHONESDK_ANDROID_ARCHS "arm64, armv7" CACHE STRING "Android architectures to build for: comma-separated list of values in [arm64, armv7, arm, x86, x86_64]")


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_ANDROID_ARCHS}" _archs)
list(GET _archs 0 _first_arch)


add_custom_target(gradle-clean ALL
	"${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}" "-DLINPHONESDK_STATE=${LINPHONESDK_STATE}" "-DLINPHONESDK_BRANCH=${LINPHONESDK_BRANCH}" "-DLINPHONESDK_FIRST_ARCH=${_first_arch}" "-P" "${LINPHONESDK_DIR}/cmake/Android/GradleClean.cmake"
)
list(APPEND LINPHONESDK_PREBUILD_DEPENDENCIES gradle-clean)


if(LINPHONESDK_PREBUILD_DEPENDENCIES)
	set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()
set(_android_build_targets)

foreach(_arch IN LISTS _archs)
	file(REMOVE_RECURSE "${CMAKE_BINARY_DIR}/linphone-sdk/android-${_arch}")

	set(_cmake_args
		"-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}/android-${_arch}"
		"-DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}/android-${_arch}"
		"-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
		"-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/android-${_arch}"
		"-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${CMAKE_SOURCE_DIR}"
		"-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-android.cmake"
		"-DCMAKE_TOOLCHAIN_FILE=toolchains/toolchain-android-${_arch}.cmake"
	)

	linphone_sdk_get_inherited_cmake_args()
	linphone_sdk_get_enable_cmake_args()
	linphone_sdk_get_sdk_cmake_args()
	list(APPEND _cmake_args ${_enable_cmake_args})
	list(APPEND _cmake_args ${_linphone_sdk_cmake_vars})

	#We have to remove the defined CMAKE_INSTALL_PREFIX from inherited variables.
	#Because cache variables take precedence and we redefine it here for multi-arch
	ExcludeFromList(_cmake_cache_args CMAKE_INSTALL_PREFIX ${_inherited_cmake_args})

	if(CMAKE_ANDROID_NDK_VERSION VERSION_EQUAL 21)
		# NDK 21b needs this but not 21
		if(CMAKE_ANDROID_NDK_VERSION_MINOR GREATER_EQUAL 1)
			ExcludeFromList(_cmake_args CMAKE_INSTALL_PREFIX)
		endif()
	endif()

	ExternalProject_Add(android-${_arch}
		${_ep_depends}
		SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake-builder"
		BINARY_DIR "${CMAKE_BINARY_DIR}/android-${_arch}"
		CMAKE_GENERATOR "${CMAKE_GENERATOR}"
		CMAKE_ARGS ${_cmake_args}
		CMAKE_CACHE_ARGS ${_cmake_cache_args}
		INSTALL_COMMAND "${CMAKE_SOURCE_DIR}/cmake/dummy.sh"
	)
	ExternalProject_Add_Step(android-${_arch} force_build
		COMMENT "Forcing build for 'android-${_arch}'"
		DEPENDEES configure
		DEPENDERS build
		ALWAYS 1
	)
	list(APPEND _android_build_targets android-${_arch})
endforeach()


add_custom_target(copy-libs ALL
	"${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DLINPHONESDK_ANDROID_ARCHS=${LINPHONESDK_ANDROID_ARCHS}" "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" "-DCMAKE_ANDROID_NDK=${CMAKE_ANDROID_NDK}" "-P" "${LINPHONESDK_DIR}/cmake/Android/CopyLibs.cmake"
	COMMENT "Copying libs"
	DEPENDS ${_android_build_targets}
)

add_custom_target(sdk ALL
	"${CMAKE_COMMAND}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}" "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" "-P" "${LINPHONESDK_DIR}/cmake/Android/GenerateSDK.cmake"
	COMMENT "Generating the SDK (zip file and aar)"
	DEPENDS copy-libs
)
