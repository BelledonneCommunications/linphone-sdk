############################################################################
# LinphoneSdkPackager.cmake
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

set(LINPHONESDK_UWP_ARCHS "x86, x64" CACHE STRING "UWP architectures to build for: comma-separated list of values in [x86, x64]")
set(LINPHONESDK_WINDOWS_ARCHS "x86, x64" CACHE STRING "Windows architectures to build for: comma-separated list of values in [x86, x64]")
set(LINPHONESDK_WINDOWSSTORE_ARCHS "x86, x64" CACHE STRING "Windows Store architectures to build for: comma-separated list of values in [x86, x64]")

get_cmake_property(vars CACHE_VARIABLES)
foreach(var ${vars})
	get_property(currentHelpString CACHE "${var}" PROPERTY HELPSTRING)
	if("${currentHelpString}" MATCHES "No help, variable specified on the command line." OR "${currentHelpString}" STREQUAL "")
		#message("${var} = [${${var}}]  --  ${currentHelpString}") # uncomment to see the variables being processed
		list(APPEND USER_ARGS "-D${var}=${${var}}")
		if("${var}" STREQUAL "CMAKE_PREFIX_PATH")
			set(PREFIX_PATH ";${${var}}")
		endif()
	endif()
endforeach()

include(ExternalProject)

if ((NOT DEFINED CMAKE_INSTALL_PREFIX) OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/linphone-sdk" CACHE PATH "Default linphone-sdk installation prefix" FORCE)
endif()

ExcludeFromList(USER_ARGS LINPHONESDK_PACKAGER ${USER_ARGS})

linphone_sdk_get_inherited_cmake_args(_inherited_cmake_args _CMAKE_BUILD_ARGS)
linphone_sdk_get_enable_cmake_args(_enable_cmake_args)
linphone_sdk_get_sdk_cmake_args(_sdk_cmake_args)
set(_cmake_args)
list(APPEND _cmake_args ${_enable_cmake_args})
list(APPEND _cmake_args ${_sdk_cmake_args})

if(LINPHONESDK_PACKAGER STREQUAL "Nuget")
	ExternalProject_Add(nuget
		SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/NuGet"
		BINARY_DIR "${CMAKE_BINARY_DIR}/nuget"
		CMAKE_GENERATOR "${CMAKE_GENERATOR}"
		CMAKE_ARGS  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DLINPHONESDK_DIR=${PROJECT_SOURCE_DIR}" ${_cmake_args}
		CMAKE_CACHE_ARGS ${_inherited_cmake_args}
		BUILD_ALWAYS 1
		INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
	)
	ExternalProject_Add_Step(nuget force_build
		COMMENT "Forcing build for 'nuget'"
		DEPENDEES configure
		DEPENDERS build
		ALWAYS 1
	)
endif()
