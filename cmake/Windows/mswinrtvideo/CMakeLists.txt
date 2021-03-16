############################################################################
# GenerateSDK.cmake
# Copyright (C) 2010-2020 Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.2)

project(uwp-mswinrtvideo)
set(MSWINRTVIDEO_SOURCE_DIR ${LINPHONESDK_DIR}/mswinrtvid/MSWinRTVideo)
#set(MSWINRTVIDEO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
if(CMAKE_GENERATOR MATCHES "^Visual Studio")
	string(REPLACE " " ";" GENERATOR_LIST "${CMAKE_GENERATOR}")
	list(GET GENERATOR_LIST 2 VS_VERSION)
else()
	if( "${MSVC_TOOLSET_VERSION}" STREQUAL "142")
		set(VS_VERSION "16")
	elseif( "${MSVC_TOOLSET_VERSION}" STREQUAL "141")
		set(VS_VERSION "15")
	elseif("${MSVC_TOOLSET_VERSION}" STREQUAL "140")
		set(VS_VERSION "14")
	elseif("${MSVC_TOOLSET_VERSION}" STREQUAL "120")
		set(VS_VERSION "12")
	elseif("${MSVC_TOOLSET_VERSION}" STREQUAL "110")
		set(VS_VERSION "11")
	elseif("${MSVC_TOOLSET_VERSION}" STREQUAL "100")
		set(VS_VERSION "10")
	elseif("${MSVC_TOOLSET_VERSION}" STREQUAL "90")
		set(VS_VERSION "9")
	else()
		set(VS_VERSION "15")
	endif()
endif()	
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/MSWinRTVideo.vcxproj.in ${CMAKE_CURRENT_BINARY_DIR}/MSWinRTVideo.vcxproj @ONLY)

message("INSTALLATION mswinrtvideo: ${CMAKE_INSTALL_PREFIX}")
message("LINPHONE_PLATFORM mswinrtvideo: ${LINPHONE_PLATFORM}")
message("CMAKE_CURRENT_BINARY_DIR mswinrtvideo: ${CMAKE_CURRENT_BINARY_DIR}")
if( CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	set(BUILD_TYPE "Release")
else()
	set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()
message("Command: msbuild MSWinRTVideo.vcxproj /p:BaseOutputPath=${CMAKE_INSTALL_PREFIX} /p:MDILCompile=true /t:build /p:Configuration=${BUILD_TYPE} /p:Platform=${LINPHONE_PLATFORM}")
execute_process(COMMAND "msbuild" "MSWinRTVideo.vcxproj" "-t:restore")
execute_process(COMMAND "msbuild" "MSWinRTVideo.vcxproj" "/p:BaseOutputPath=${CMAKE_INSTALL_PREFIX}" "/p:MDILCompile=true" "/t:build" "/p:Configuration=${BUILD_TYPE}" "/p:Platform=${LINPHONE_PLATFORM}")
