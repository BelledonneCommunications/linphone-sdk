############################################################################
# CMakeLists.txt
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

cmake_minimum_required(VERSION 3.22)

project(cs-wrapper)

configure_file(${LINPHONESDK_INSTALL_DIR}/share/linphonecs/LinphoneWrapper.cs ${CMAKE_CURRENT_BINARY_DIR} COPYONLY)
configure_file(${LINPHONESDK_DIR}/cmake/Windows/wrapper/Cs${WINDOWS_VARIANT}Wrapper.csproj.in ${CMAKE_CURRENT_BINARY_DIR}/Cs${WINDOWS_VARIANT}Wrapper.csproj @ONLY)
configure_file(${LINPHONESDK_DIR}/cmake/Windows/wrapper/Properties/AssemblyInfo.cs ${CMAKE_CURRENT_BINARY_DIR}/Properties/AssemblyInfo.cs COPYONLY)
configure_file(${LINPHONESDK_DIR}/cmake/Windows/wrapper/Properties/CsWrapper.rd.xml ${CMAKE_CURRENT_BINARY_DIR}/Properties/CsWrapper.rd.xml COPYONLY)

message("INSTALLATION: ${LINPHONESDK_INSTALL_DIR}")
message("LINPHONESDK_DIR: ${LINPHONESDK_DIR}")
message("LINPHONE_PLATFORM: ${LINPHONE_PLATFORM}")
message("BUILD_TYPE: ${BUILD_TYPE}")
message("WINDOWS_VARIANT: ${WINDOWS_VARIANT}")

message("Command : msbuild Cs${WINDOWS_VARIANT}Wrapper.csproj /p:BaseOutputPath=${LINPHONESDK_INSTALL_DIR} /p:MDILCompile=true /t:build /p:Configuration=${BUILD_TYPE} /p:Platform=${LINPHONE_PLATFORM}")

add_custom_target(cs-wrapper-target ALL
	COMMAND ${CMAKE_COMMAND} -E echo "Restoring wrapper project"
	COMMAND "msbuild" "Cs${WINDOWS_VARIANT}Wrapper.csproj" "-t:restore"
	COMMAND ${CMAKE_COMMAND} -E echo "Building wrapper : msbuild Cs${WINDOWS_VARIANT}Wrapper.csproj /p:BaseOutputPath=${LINPHONESDK_INSTALL_DIR} /p:MDILCompile=true /t:build /p:Configuration=${BUILD_TYPE} /p:Platform=${LINPHONE_PLATFORM}"
	COMMAND "msbuild" "Cs${WINDOWS_VARIANT}Wrapper.csproj" "/p:BaseOutputPath=${LINPHONESDK_INSTALL_DIR}" "/p:MDILCompile=true" "/t:build" "/p:Configuration=${BUILD_TYPE}" "/p:Platform=${LINPHONE_PLATFORM}"
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	USES_TERMINAL
)
