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

project(uwp-wrapper)


file(COPY ${CMAKE_INSTALL_PREFIX}/share/linphonecs/LinphoneWrapper.cs DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${LINPHONESDK_DIR}/cmake/Windows/wrapper/CsUwpWrapper.csproj DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${LINPHONESDK_DIR}/cmake/Windows/wrapper/Properties DESTINATION ${CMAKE_CURRENT_BINARY_DIR})



message("INSTALLATION: ${CMAKE_INSTALL_PREFIX}")
message("LINPHONESDK_DIR: ${LINPHONESDK_DIR}")
message("LINPHONE_PLATFORM: ${LINPHONE_PLATFORM}")

if( NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	set(BUILD_TYPE "Release")
else()
	set(BUILD_TYPE ${CMAKE_BUILD_TYPE})
endif()

message("Command : msbuild CsUwpWrapper.csproj /p:BaseOutputPath=${CMAKE_INSTALL_PREFIX} /p:MDILCompile=true /t:build /p:Configuration=${BUILD_TYPE} /p:Platform=${LINPHONE_PLATFORM}")
execute_process(COMMAND "msbuild" "CsUwpWrapper.csproj" "-t:restore")
execute_process(COMMAND "msbuild" "CsUwpWrapper.csproj" "/p:BaseOutputPath=${CMAKE_INSTALL_PREFIX}" "/p:MDILCompile=true" "/t:build" "/p:Configuration=${BUILD_TYPE}" "/p:Platform=${LINPHONE_PLATFORM}")
