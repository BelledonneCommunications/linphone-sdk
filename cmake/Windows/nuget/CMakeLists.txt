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

project(uwp-nuget)
#Configuration files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.nuspec.in ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.nuspec @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.props.in ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.props @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/LinphoneSDK.targets.in ${CMAKE_CURRENT_BINARY_DIR}/LinphoneSDK.targets @ONLY)
if(LINPHONESDK_OUTPUT_DIR)
	file(TO_NATIVE_PATH "${LINPHONESDK_OUTPUT_DIR}" WINDOWS_INSTALL_PATH)
else()
	file(TO_NATIVE_PATH "${CMAKE_INSTALL_PREFIX}" WINDOWS_INSTALL_PATH)
endif()

#Nuget binary
find_program(NUGET_PROGRAM nuget.exe)
if(NUGET_PROGRAM-NOTFOUND)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/nuget.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	set(NUGET_PROGRAM "nuget.exe")
endif()

add_custom_target(nuget ALL)

#add_custom_command(TARGET nuget PRE_BUILD
#	COMMAND ${CMAKE_COMMAND} -E rm -rRf "${CMAKE_CURRENT_BINARY_DIR}/packages"
#)
	
#message(${NUGET_PROGRAM} "pack" "LinphoneSDK.nuspec" "-Properties" "OutputSdkDir=${WINDOWS_INSTALL_PATH}" "-OutputDirectory" "packages")
add_custom_command(TARGET nuget
	COMMAND ${NUGET_PROGRAM} "pack" "LinphoneSDK.nuspec" "-Properties" "OutputSdkDir=${WINDOWS_INSTALL_PATH}" "-OutputDirectory" "packages"
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
# "-OutputDirectory" "${WINDOWS_INSTALL_PATH}"
#install(FILES "*.nupkg" DESTINATION ${CMAKE_INSTALL_PREFIX})

install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/packages"
        DESTINATION ${CMAKE_INSTALL_PREFIX}
        FILES_MATCHING PATTERN "*.nupkg"
)
