############################################################################
# LinphoneSdkPlatformUWP.cmake
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
include(LinphoneSdkCheckBuildToolsUWP)


set(LINPHONESDK_UWP_ARCHS "ARM, x64, x86" CACHE STRING "UWP architectures to build for: comma-separated list of values in [ARM, x64, x86]")


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_UWP_ARCHS}" _archs)
list(GET _archs 0 _first_arch)


if(LINPHONESDK_PREBUILD_DEPENDENCIES)
	set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()
set(_uwp_build_targets)

foreach(_arch IN LISTS _archs)
	set(_cmake_args
		"-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/linphone-sdk/uwp-${_arch}"
		"-DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/linphone-sdk/uwp-${_arch}"
		"-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
		"-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/uwp-${_arch}"
		"-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${CMAKE_SOURCE_DIR}"
		"-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-uwp.cmake"
		"-DCMAKE_CROSSCOMPILING=YES"
		"-DCMAKE_SYSTEM_NAME=WindowsStore"
		"-DCMAKE_SYSTEM_VERSION=10.0"
		"-DCMAKE_SYSTEM_PROCESSOR=${_arch}"
		"-DCMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD=TRUE"
	)

	linphone_sdk_get_inherited_cmake_args()
	linphone_sdk_get_enable_cmake_args()
	list(APPEND _cmake_args ${_enable_cmake_args})

	ExternalProject_Add(uwp-${_arch}
		${_ep_depends}
		SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake-builder"
		BINARY_DIR "${CMAKE_BINARY_DIR}/uwp-${_arch}"
		CMAKE_GENERATOR "${CMAKE_GENERATOR}"
		CMAKE_ARGS ${_cmake_args}
		CMAKE_CACHE_ARGS ${_inherited_cmake_args}
		INSTALL_COMMAND "${CMAKE_SOURCE_DIR}/cmake/dummy.sh"
	)
	ExternalProject_Add_Step(uwp-${_arch} force_build
		COMMENT "Forcing build for 'uwp-${_arch}'"
		DEPENDEES configure
		DEPENDERS build
		ALWAYS 1
	)
	list(APPEND _uwp_build_targets uwp-${_arch})
endforeach()
