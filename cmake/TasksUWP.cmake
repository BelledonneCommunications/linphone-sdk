############################################################################
# TasksUWP.cmake
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


set(LINPHONESDK_UWP_ARCHS "x86, x64" CACHE STRING "UWP architectures to build: comma-separated list of values in [x86, x64]")


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_UWP_ARCHS}" _UWP_ARCHS)


############################################################################
# Build each selected architecture
############################################################################

linphone_sdk_get_inherited_cmake_args(_CMAKE_CONFIGURE_ARGS _CMAKE_BUILD_ARGS)
linphone_sdk_get_enable_cmake_args(_UWP_CMAKE_ARGS)

# Tools have already been checked while running CMake for the general uwp project, do not run it again for each architecture
list(APPEND _UWP_CMAKE_ARGS "-DENABLE_WINDOWS_TOOLS_CHECK=OFF")

foreach(_UWP_ARCH IN LISTS _UWP_ARCHS)
	set(_UWP_ARCH_BINARY_DIR "${PROJECT_BINARY_DIR}/uwp-${_UWP_ARCH}")
	set(_UWP_ARCH_INSTALL_DIR "${PROJECT_BINARY_DIR}/linphone-sdk/uwp-${_UWP_ARCH}")
  add_custom_target(uwp-${_UWP_ARCH} ALL
    COMMAND ${CMAKE_COMMAND} --preset=uwp-${_UWP_ARCH} -B ${_UWP_ARCH_BINARY_DIR} ${_CMAKE_CONFIGURE_ARGS} ${_UWP_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${_UWP_ARCH_INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} --build ${_UWP_ARCH_BINARY_DIR} --target install ${_CMAKE_BUILD_ARGS}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Building Linphone SDK for UWP ${_UWP_ARCH}"
    USES_TERMINAL
    COMMAND_EXPAND_LISTS
  )
endforeach()


############################################################################
# Build CSharp wrapper for each selected architecture
############################################################################

set(_UWP_WRAPPER_TARGETS)
foreach(_UWP_ARCH IN LISTS _UWP_ARCHS)
  ExternalProject_add(uwp-wrapper-${_UWP_ARCH}
		SOURCE_DIR "${PROJECT_SOURCE_DIR}/cmake/Windows/wrapper"
		BINARY_DIR "${PROJECT_BINARY_DIR}/uwp-${_UWP_ARCH}/CsWrapper"
		CMAKE_ARGS "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}" "-DLINPHONESDK_INSTALL_DIR=${PROJECT_BINARY_DIR}/linphone-sdk/uwp-${_UWP_ARCH}" "-DLINPHONE_PLATFORM=${_UWP_ARCH}" "-DWINDOWS_VARIANT=Uwp" "-DBUILD_TYPE=$<IF:$<CONFIG:Debug>,Debug,Release>" ${_UWP_CMAKE_ARGS}
    DEPENDS uwp-${_UWP_ARCH}
		BUILD_COMMAND ${CMAKE_COMMAND} -E echo ""
		INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
  )
  list(APPEND _UWP_WRAPPER_TARGETS uwp-wrapper-${_UWP_ARCH})
endforeach()

############################################################################
# Generate the SDK
############################################################################

add_custom_target(sdk ALL
	COMMAND "${CMAKE_COMMAND}"
    "-DLINPHONESDK_PLATFORM=${LINPHONESDK_PLATFORM}"
    "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
    "-DLINPHONESDK_BUILD_DIR=${PROJECT_BINARY_DIR}"
    "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
    "-DLINPHONESDK_FOLDER=linphone-sdk"
    "-DLINPHONESDK_WINDOWS_ARCH=uwp"
    "-DENABLE_EMBEDDED_OPENH264=${ENABLE_EMBEDDED_OPENH264}"
    "-P" "${PROJECT_SOURCE_DIR}/cmake/Windows/GenerateSDK.cmake"
	DEPENDS ${_UWP_WRAPPER_TARGETS}
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	COMMENT "Generating the SDK (zip file)"
  USES_TERMINAL
)
