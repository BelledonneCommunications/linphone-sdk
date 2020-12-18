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


#set(LINPHONESDK_UWP_ARCHS "ARM, x64, x86" CACHE STRING "UWP architectures to build for: comma-separated list of values in [ARM, x64, x86]")
#set(LINPHONESDK_UWP_ARCHS "x86, x64" CACHE STRING "UWP architectures to build for: comma-separated list of values in [ARM, x64, x86]")
set(LINPHONESDK_UWP_ARCHS "x64" CACHE STRING "UWP architectures to build for: comma-separated list of values in [x64]")
#set(LINPHONESDK_UWP_ARCHS "x86" CACHE STRING "UWP architectures to build for: comma-separated list of values in [ARM, x64, x86]")

linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_UWP_ARCHS}" _archs)
list(GET _archs 0 _first_arch)

if(LINPHONESDK_PREBUILD_DEPENDENCIES)
        set(_ep_depends DEPENDS ${LINPHONESDK_PREBUILD_DEPENDENCIES})
endif()
set(_uwp_build_targets)
foreach(_arch IN LISTS _archs)

        set(_cmake_args
                "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}/uwp-${_arch}"
                "-DCMAKE_PREFIX_PATH=${CMAKE_INSTALL_PREFIX}/uwp-${_arch}"
                "-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON"
                "-DLINPHONE_BUILDER_WORK_DIR=${CMAKE_BINARY_DIR}/WORK/uwp-${_arch}"
                "-DLINPHONE_BUILDER_EXTERNAL_SOURCE_PATH=${CMAKE_SOURCE_DIR}"
                "-DLINPHONE_BUILDER_CONFIG_FILE=configs/config-uwp.cmake"
                "-DCMAKE_CROSSCOMPILING=YES"
                "-DCMAKE_SYSTEM_NAME=WindowsStore"
                "-DCMAKE_SYSTEM_VERSION=10.0"
                "-DCMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD=TRUE"
                "-DCMAKE_VS_WINRT_BY_DEFAULT=1"
                "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        )
        if( _arch STREQUAL "x64")
                list(APPEND _cmake_args "-DCMAKE_SYSTEM_PROCESSOR=x86_64")
                #list(APPEND _cmake_args "-DCMAKE_GENERATOR_PLATFORM=x64")
                set(SYSTEM_GENERATOR "${CMAKE_GENERATOR} Win64")#Only for VS 2017
                #set(SYSTEM_GENERATOR "${CMAKE_GENERATOR}")
        else()
                list(APPEND _cmake_args "-DCMAKE_SYSTEM_PROCESSOR=${_arch}")
                list(APPEND _cmake_args "-DCMAKE_GENERATOR_PLATFORM=Win32")
                set(SYSTEM_GENERATOR "${CMAKE_GENERATOR}")
        endif()
        linphone_sdk_get_inherited_cmake_args()
        linphone_sdk_get_enable_cmake_args()
        list(APPEND _cmake_args ${_enable_cmake_args})

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
        message("${_arch} : ${_cmake_args}")
        ExternalProject_Add(uwp-${_arch}
                ${_ep_depends}
                SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake-builder"
                BINARY_DIR "${CMAKE_BINARY_DIR}/uwp-${_arch}"
                CMAKE_GENERATOR "${SYSTEM_GENERATOR}"
                CMAKE_ARGS ${_cmake_args}
                #CMAKE_CACHE_ARGS ${_cmake_cache_args}
                INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
        )
        ExternalProject_Add_Step(uwp-${_arch} force_build
                COMMENT "Forcing build for 'uwp-${_arch}'"
                DEPENDEES configure
                DEPENDERS build
                ALWAYS 1
        )

        ExternalProject_Add(uwp-wrapper-${_arch}
                DEPENDS uwp-${_arch}
                SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/Windows/wrapper"
                BINARY_DIR "${CMAKE_BINARY_DIR}/uwp-${_arch}/wrapper"
                CMAKE_GENERATOR "${SYSTEM_GENERATOR}"
                CMAKE_ARGS "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_BUILD_DIR=${CMAKE_BINARY_DIR}/uwp-${_arch}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}" "-DLINPHONESDK_WINDOWS_BASE_URL=${LINPHONESDK_WINDOWS_BASE_URL}" "-DLINPHONE_PLATFORM=${_arch}" ${_cmake_args}
                #CMAKE_CACHE_ARGS ${_cmake_cache_args}
                BUILD_COMMAND ${CMAKE_COMMAND} -E echo ""
                INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
        )

        list(APPEND _uwp_build_targets uwp-${_arch} uwp-wrapper-${_arch})
endforeach()

ExternalProject_Add(uwp-nuget
        DEPENDS ${_uwp_build_targets}
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/Windows/nuget"
        BINARY_DIR "${CMAKE_BINARY_DIR}/uwp-nuget"
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_ARGS  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
#	INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Install step will not be done by external project"
        CMAKE_CACHE_ARGS ${_cmake_cache_args}
        BUILD_ALWAYS 1
)
ExternalProject_Add(uwp-nuget-only
        SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/Windows/nuget"
        BINARY_DIR "${CMAKE_BINARY_DIR}/uwp-nuget"
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_ARGS  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
#	INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Install step will not be done by external project"
        CMAKE_CACHE_ARGS ${_cmake_cache_args}
        BUILD_ALWAYS 1
        EXCLUDE_FROM_ALL 1
)
