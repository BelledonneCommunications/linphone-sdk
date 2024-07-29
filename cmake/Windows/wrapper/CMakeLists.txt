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


file(COPY ${LINPHONESDK_INSTALL_DIR}/share/linphonecs/LinphoneWrapper.cs DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${LINPHONESDK_DIR}/cmake/Windows/wrapper/Cs${WINDOWS_VARIANT}Wrapper.csproj DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${LINPHONESDK_DIR}/cmake/Windows/wrapper/Properties DESTINATION ${CMAKE_CURRENT_BINARY_DIR})



message("INSTALLATION: ${LINPHONESDK_INSTALL_DIR}")
message("LINPHONESDK_DIR: ${LINPHONESDK_DIR}")
message("LINPHONE_PLATFORM: ${LINPHONE_PLATFORM}")
message("BUILD_TYPE: ${BUILD_TYPE}")
message("WINDOWS_VARIANT: ${WINDOWS_VARIANT}")

message("Command : msbuild Cs${WINDOWS_VARIANT}Wrapper.csproj /p:BaseOutputPath=${LINPHONESDK_INSTALL_DIR} /p:MDILCompile=true /t:build /p:Configuration=${BUILD_TYPE} /p:Platform=${LINPHONE_PLATFORM}")
execute_process(COMMAND "msbuild" "Cs${WINDOWS_VARIANT}Wrapper.csproj" "-t:restore")
execute_process(COMMAND "msbuild" "Cs${WINDOWS_VARIANT}Wrapper.csproj" "/p:BaseOutputPath=${LINPHONESDK_INSTALL_DIR}" "/p:MDILCompile=true" "/t:build" "/p:Configuration=${BUILD_TYPE}" "/p:Platform=${LINPHONE_PLATFORM}")
