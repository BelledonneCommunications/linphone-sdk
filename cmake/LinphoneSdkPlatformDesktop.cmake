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


set(_cmake_args
	"-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/linphone-sdk/desktop"
	"-DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/linphone-sdk/desktop"
	"-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
	"-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/desktop"
	"-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${CMAKE_SOURCE_DIR}"
	"-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-desktop.cmake"
)

linphone_sdk_get_inherited_cmake_args()
linphone_sdk_get_enable_cmake_args()
list(APPEND _cmake_args ${_enable_cmake_args})

if(LINPHONESDK_PREBUILD_DEPENDENCIES)
	set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()

if(WIN32)
	set(_install_command "${CMAKE_SOURCE_DIR}/cmake/dummy.bat")
else()
	set(_install_command "${CMAKE_SOURCE_DIR}/cmake/dummy.sh")
endif()

ExternalProject_Add(sdk
	${_ep_depends}
	SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake-builder"
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
