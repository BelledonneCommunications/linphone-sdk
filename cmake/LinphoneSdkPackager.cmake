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
if(LINPHONESDK_PACKAGER STREQUAL "Nuget")
	if(LINPHONESDK_DESKTOP_ZIP_PATH OR LINPHONESDK_UWP_ZIP_PATH OR LINPHONESDK_WINDOWSSTORE_ZIP_PATH)
		project(nuget)
		set(LINPHONESDK_OUTPUT_DIR ${CMAKE_BINARY_DIR}/WORK/packages/nuget)
		add_custom_target( unzip ALL)
		add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E remove_directory ${LINPHONESDK_OUTPUT_DIR})
		add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/desktop)
		add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/uwp)
		add_custom_command(TARGET unzip PRE_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory ${LINPHONESDK_OUTPUT_DIR}/windowsstore)
		
		set(NUSPEC_HAVE "")
		if(LINPHONESDK_DESKTOP_ZIP_PATH)
			list(APPEND NUSPEC_HAVE "-DNUSPEC_HAVE_WIN32=1")
			message(STATUS "Retrieve Desktop build")
		endif()
		if(LINPHONESDK_UWP_ZIP_PATH)
			list(APPEND NUSPEC_HAVE "-DNUSPEC_HAVE_UWP=1")
			message(STATUS "Retrieve UWP build")
		endif()
		if(LINPHONESDK_WINDOWSSTORE_ZIP_PATH)
			list(APPEND NUSPEC_HAVE "-DNUSPEC_HAVE_WINDOWSSTORE=1")
			message(STATUS "Retrieve Windows Store build")
		endif()
		FILE(GLOB DESKTOP_ZIP_FILES ${LINPHONESDK_DESKTOP_ZIP_PATH}/*.zip)
		foreach(item ${DESKTOP_ZIP_FILES})
			add_custom_command(TARGET unzip PRE_BUILD
				COMMAND ${CMAKE_COMMAND} -E tar xzf ${item}
				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/WORK/packages/nuget/desktop
				COMMENT "Unzipping files : ${item}\n"
				VERBATIM)
		endforeach(item)
		
		FILE(GLOB UWP_ZIP_FILES ${LINPHONESDK_UWP_ZIP_PATH}/*.zip)
		foreach(item ${UWP_ZIP_FILES})
			add_custom_command(TARGET unzip PRE_BUILD
				COMMAND ${CMAKE_COMMAND} -E tar xzf ${item}
				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/WORK/packages/nuget/uwp
				COMMENT "Unzipping files : ${item}"
				VERBATIM)
		endforeach(item)
		
		FILE(GLOB WINSTORE_ZIP_FILES ${LINPHONESDK_WINDOWSSTORE_ZIP_PATH}/*.zip)
		foreach(item ${WINSTORE_ZIP_FILES})
			add_custom_command(TARGET unzip PRE_BUILD
				COMMAND ${CMAKE_COMMAND} -E tar xzf ${item}
				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/WORK/packages/nuget/windowsstore
				COMMENT "Unzipping files : ${item}"
				VERBATIM)
		endforeach(item)
		
#--------------		Desktop x86		
#		FILE(GLOB DESKTOP_ZIP_FILES ${LINPHONESDK_DESKTOP_ZIP_PATH}/*.zip)
#		foreach(item ${DESKTOP_ZIP_FILES})
#			list(APPEND ALL_ZIP_FILES ${item})
#		endforeach(item) 
#--------------		UWP x64		
#		FILE(GLOB UWP_ZIP_FILES ${LINPHONESDK_UWP_ZIP_PATH}/*.zip)
#		foreach(item ${UWP_ZIP_FILES})
#			list(APPEND ALL_ZIP_FILES ${item})
#		endforeach(item)
#		list(REMOVE_DUPLICATES ALL_ZIP_FILES)
#		foreach(item ${ALL_ZIP_FILES})
#			add_custom_command(TARGET unzip PRE_BUILD
#				COMMAND ${CMAKE_COMMAND} -E tar xzf ${item}
#				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/WORK/packages/nuget
#				COMMENT "Unzipping files : ${item}"
#				VERBATIM)
#		endforeach(item)
		
		ExternalProject_Add(uwp-nuget
		        DEPENDS unzip
		        SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/Windows/nuget"
		        BINARY_DIR "${CMAKE_BINARY_DIR}/uwp-nuget"
		        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
		        CMAKE_ARGS  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}" "-DLINPHONESDK_OUTPUT_DIR=${LINPHONESDK_OUTPUT_DIR}" ${NUSPEC_HAVE}
		        CMAKE_CACHE_ARGS ${_cmake_cache_args}
		        BUILD_ALWAYS 1
		)
	ExternalProject_Add_Step(uwp-nuget force_build
			COMMENT "Forcing build for 'uwp-nuget'"
			DEPENDEES configure
			DEPENDERS build
			ALWAYS 1
	)
	else()
		message(FATAL_ERROR "You need specify LINPHONESDK_DESKTOP_ZIP_PATH or LINPHONESDK_UWP_ZIP_PATH")
	set(_packager_depends)
	
#--------------		Desktop x86

	set(_cmake_args "-DLINPHONESDK_PLATFORM=Desktop" 
		"-AWin32" 
		"-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" 
		"-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}/nuget-x86"
		"-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
	ExternalProject_Add(nuget-x86
			SOURCE_DIR "${CMAKE_SOURCE_DIR}/"
			BINARY_DIR "${CMAKE_BINARY_DIR}/nuget-x86"
			CMAKE_GENERATOR "${SYSTEM_GENERATOR}"
			CMAKE_ARGS ${_cmake_args} ${USER_ARGS}
			INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
	)
	ExternalProject_Add_Step(nuget-x86 force_build
			COMMENT "Forcing build for 'nuget-x86'"
			DEPENDEES configure
			DEPENDERS build
			ALWAYS 1
	)
	list(APPEND _packager_depends nuget-x86)
	
#--------------		UWP x64

	set(_cmake_args "-DLINPHONESDK_PLATFORM=UWP" 
		"-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}" 
		"-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}/../nuget-uwp"
		"-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")
	ExternalProject_Add(nuget-uwp
			SOURCE_DIR "${CMAKE_SOURCE_DIR}/"
			BINARY_DIR "${CMAKE_BINARY_DIR}/nuget-uwp"
			CMAKE_GENERATOR "${SYSTEM_GENERATOR}"
			CMAKE_ARGS ${_cmake_args} ${USER_ARGS}
			INSTALL_COMMAND ${CMAKE_COMMAND} -E echo ""
	)
	ExternalProject_Add_Step(nuget-uwp force_build
			COMMENT "Forcing build for 'nuget-uwp'"
			DEPENDEES configure
			DEPENDERS build
			ALWAYS 1
	)
	list(APPEND _packager_depends nuget-uwp)
	
#--------------		PACKAGER

	ExternalProject_Add(nuget-packager
			DEPENDS ${_packager_depends}
			SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/Windows/nuget"
			BINARY_DIR "${CMAKE_BINARY_DIR}/nuget-packager"
			CMAKE_ARGS  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
	#	INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Install step will not be done by external project"
			CMAKE_CACHE_ARGS ${_cmake_cache_args}
			BUILD_ALWAYS 1
	)
	ExternalProject_Add(nuget-packager-only
			SOURCE_DIR "${CMAKE_SOURCE_DIR}/cmake/Windows/nuget"
			BINARY_DIR "${CMAKE_BINARY_DIR}/nuget-packager"
			CMAKE_ARGS  "-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" "-DLINPHONESDK_DIR=${LINPHONESDK_DIR}" "-DLINPHONESDK_VERSION=${LINPHONESDK_VERSION}"
	#	INSTALL_COMMAND ${CMAKE_COMMAND} -E echo "Install step will not be done by external project"
			CMAKE_CACHE_ARGS ${_cmake_cache_args}
			BUILD_ALWAYS 1
			EXCLUDE_FROM_ALL 1
	)
endif()
endif()
