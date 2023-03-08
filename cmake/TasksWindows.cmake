############################################################################
# TasksWindows.cmake
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


set(LINPHONESDK_WINDOWS_ARCHS "32bits, 64bits" CACHE STRING "Windows architectures to build: comma-separated list of values in [32bits, 64bits]")
set(LINPHONESDK_WINDOWS_BASE_URL "https://www.linphone.org/releases/windows/sdk" CACHE STRING "URL of the repository where the Windows SDK zip files are located")
set(LINPHONESDK_WINDOWS_PRESET_PREFIX "windows-" CACHE STRING "Prefix of the presets to use to build each architectures")


linphone_sdk_convert_comma_separated_list_to_cmake_list("${LINPHONESDK_WINDOWS_ARCHS}" _WINDOWS_ARCHS)


############################################################################
# Build each selected architecture
############################################################################

linphone_sdk_get_inherited_cmake_args(_CMAKE_CONFIGURE_ARGS _CMAKE_BUILD_ARGS)
linphone_sdk_get_enable_cmake_args(_WINDOWS_CMAKE_ARGS)

# Tools have already been checked while running CMake for the general windows project, do not run it again for each architecture
list(APPEND _WINDOWS_CMAKE_ARGS "-DENABLE_WINDOWS_TOOLS_CHECK=OFF")

set(_WINDOWS_TARGETS)
foreach(_WINDOWS_ARCH IN LISTS _WINDOWS_ARCHS)
	if(ENABLE_MICROSOFT_STORE_APP)
		set(_TARGET "win-store-${_WINDOWS_ARCH}")
		set(_PRESET "windows-store-${_WINDOWS_ARCH}")
		set(_NAME "Windows")
	else()
		set(_TARGET "win-${_WINDOWS_ARCH}")
		set(_PRESET "${LINPHONESDK_WINDOWS_PRESET_PREFIX}${_WINDOWS_ARCH}")
		set(_NAME "Windows Store")
	endif()
	add_custom_target(${_TARGET} ALL
	  COMMAND ${CMAKE_COMMAND} --preset=${_PRESET} ${_CMAKE_CONFIGURE_ARGS} ${_WINDOWS_CMAKE_ARGS}
	  COMMAND ${CMAKE_COMMAND} --build --preset=${_PRESET} --target install ${_CMAKE_BUILD_ARGS}
	  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
	  COMMENT "Building Linphone SDK for ${_NAME} ${_WINDOWS_ARCH}"
	  USES_TERMINAL
	  COMMAND_EXPAND_LISTS
	)
  list(APPEND _WINDOWS_TARGETS ${_TARGET})
endforeach()


############################################################################
# Generate the SDK for each selected architecture
############################################################################

foreach(_WINDOWS_ARCH IN LISTS _WINDOWS_ARCHS)
	if(ENABLE_MICROSOFT_STORE_APP)
		set(_DEPENDS_TARGET win-store-${_WINDOWS_ARCH})
		set(_NAME "Windows")
		if(_WINDOWS_ARCH STREQUAL "32bits")
			set(LINPHONESDK_PLATFORM "win32store")
		else()
			set(LINPHONESDK_PLATFORM "win64store")
		endif()
	else()
		set(_DEPENDS_TARGET win-${_WINDOWS_ARCH})
		set(_NAME "Windows Store")
		if(_WINDOWS_ARCH STREQUAL "32bits")
			set(LINPHONESDK_PLATFORM "win32")
		else()
			set(LINPHONESDK_PLATFORM "win64")
		endif()
	endif()
	
	add_custom_target(sdk-${_WINDOWS_ARCH} ALL
		COMMAND "${CMAKE_COMMAND}"
	    "-DLINPHONESDK_PLATFORM=${LINPHONESDK_PLATFORM}"
	    "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}"
	    "-DLINPHONESDK_BUILD_DIR=${PROJECT_BINARY_DIR}"
	    "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
	    "-DLINPHONESDK_WINDOWS_BASE_URL=${LINPHONESDK_WINDOWS_BASE_URL}"
			"-DLINPHONESDK_FOLDER=linphone-sdk/${LINPHONESDK_PLATFORM}"
	    "-DENABLE_EMBEDDED_OPENH264=${ENABLE_EMBEDDED_OPENH264}"
	    "-P" "${PROJECT_SOURCE_DIR}/cmake/Windows/GenerateSDK.cmake"
	  DEPENDS ${_DEPENDS_TARGET}
	  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
		COMMENT "Generating the SDK (zip file) for ${_NAME} ${_WINDOWS_ARCH}"
	  USES_TERMINAL
	)
endforeach()
