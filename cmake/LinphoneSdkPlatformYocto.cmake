################################################################################
#
#  Copyright (c) 2010-2022 Belledonne Communications SARL.
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


set(_desktop_install_prefix "${CMAKE_BINARY_DIR}/linphone-sdk/desktop")
set(_desktop_prefix_path ${_desktop_install_prefix} ${CMAKE_PREFIX_PATH})
string(REPLACE ";" "|" _desktop_prefix_path "${_desktop_prefix_path}")
set(_cmake_args
	"-DCMAKE_INSTALL_PREFIX=${_desktop_install_prefix}"
	"-DCMAKE_PREFIX_PATH=${_desktop_prefix_path}"
	"-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
	"-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/desktop"
	"-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${PROJECT_SOURCE_DIR}"
	"-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-yocto.cmake"
	"-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
)

linphone_sdk_get_inherited_cmake_args()
linphone_sdk_get_enable_cmake_args()
linphone_sdk_get_sdk_cmake_args()
list(APPEND _cmake_args ${_enable_cmake_args})
list(APPEND _cmake_args ${_linphone_sdk_cmake_vars})

if(LINPHONESDK_PREBUILD_DEPENDENCIES)
	set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()

set(_install_command "${PROJECT_SOURCE_DIR}/cmake/dummy.sh")

ExternalProject_Add(sdk
	${_ep_depends}
	SOURCE_DIR "${PROJECT_SOURCE_DIR}/cmake-builder"
	BINARY_DIR "${CMAKE_BINARY_DIR}/desktop"
	CMAKE_GENERATOR "${CMAKE_GENERATOR}"
	CMAKE_ARGS ${_cmake_args}
	CMAKE_CACHE_ARGS ${_inherited_cmake_args}
	INSTALL_COMMAND ${_install_command}
)
ExternalProject_Add_Step(sdk force_build
	COMMENT "Forcing build for 'desktop'"
	DEPENDEES configure
	DEPENDERS build
	ALWAYS 1
)
