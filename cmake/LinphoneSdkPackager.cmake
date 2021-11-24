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

cmake_minimum_required(VERSION 3.1)

get_cmake_property(vars CACHE_VARIABLES)
foreach(var ${vars})
	get_property(currentHelpString CACHE "${var}" PROPERTY HELPSTRING)
	if("${currentHelpString}" MATCHES "No help, variable specified on the command line." OR "${currentHelpString}" STREQUAL "")
		#message("${var} = [${${var}}]  --  ${currentHelpString}") # uncomment to see the variables being processed
		list(APPEND USER_ARGS "-D${var}=${${var}}")
		if( "${var}" STREQUAL "CMAKE_PREFIX_PATH")
			set(PREFIX_PATH ";${${var}}")
		endif()
	endif()
endforeach()

include(LinphoneSdkCheckBuildToolsCommon)
include(ExternalProject)

if ((NOT DEFINED CMAKE_INSTALL_PREFIX) OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/linphone-sdk" CACHE PATH "Default linphone-sdk installation prefix" FORCE)
endif()

ExcludeFromList(USER_ARGS LINPHONESDK_PACKAGER ${USER_ARGS})

linphone_sdk_get_inherited_cmake_args()
linphone_sdk_get_enable_cmake_args()
linphone_sdk_get_sdk_cmake_args()
list(APPEND _cmake_args ${_enable_cmake_args})
list(APPEND _cmake_args ${_linphone_sdk_cmake_vars})

if(LINPHONESDK_PACKAGER STREQUAL "Nuget")
	set(_forwarded_var_names
		# Xamarin
		LINPHONESDK_CSHARP_WRAPPER_PATH
		LINPHONESDK_ANDROID_AAR_PATH
		LINPHONESDK_IOS_FRAMEWORKS_PATH
		# Windows
		LINPHONESDK_DESKTOP_ZIP_PATH
		LINPHONESDK_UWP_ZIP_PATH
		LINPHONESDK_WINDOWSSTORE_ZIP_PATH
	)
	foreach(_varname ${_forwarded_var_names})
		if (DEFINED ${_varname})
			list(APPEND _forwarded "-D${_varname}=${${_varname}}")
		endif()
	endforeach()

	ExternalProject_Add(nuget
		SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/NuGet"
		BINARY_DIR "${CMAKE_BINARY_DIR}/nuget"
		CMAKE_GENERATOR "${CMAKE_GENERATOR}"
		CMAKE_ARGS  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_STATE=${LINPHONESDK_STATE}" "-DLINPHONESDK_OUTPUT_DIR=${LINPHONESDK_OUTPUT_DIR}" ${_forwarded} ${_cmake_args}
		CMAKE_CACHE_ARGS ${_inherited_cmake_args}
		BUILD_ALWAYS 1
	)
	ExternalProject_Add_Step(nuget force_build
		COMMENT "Forcing build for 'nuget'"
		DEPENDEES configure
		DEPENDERS build
		ALWAYS 1
	)
endif()
