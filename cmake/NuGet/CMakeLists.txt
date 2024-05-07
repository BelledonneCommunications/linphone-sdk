############################################################################
#
#  Copyright (c) 2021 Belledonne Communications SARL.
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <http://www.gnu.org/licenses/>.
#
############################################################################

cmake_minimum_required(VERSION 3.22)

project(linphonesdk-nuget NONE)

if(NOT LINPHONESDK_VERSION)
	message(FATAL_ERROR "Please specify LINPHONESDK_VERSION. This will be used to derive the version of the `.nupkg`.")
endif()

# Workaround Gitlab's NuGet registry lookup by version being incompatible with NuGet clients
# https://gitlab.com/gitlab-org/gitlab/-/issues/270976
string(REGEX REPLACE "\\+[0-9a-f]+" "" NUPKG_VERSION ${LINPHONESDK_VERSION})

string(TIMESTAMP CURRENT_YEAR "%Y")

# Icon copied from:
# https://gitlab.linphone.org/BC/public/linphone-desktop/-/raw/08ca4791c0ccb5dd1693a878b975fd347a8cf490/linphone-app/assets/icons/hicolor/128x128/apps/icon.png
set(NUGET_ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/icon.png)


if(WIN32) # Because of lack of testing, we must keep the old Windows nuget

	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/shared-nuspec-metadata.in.xml
		${CMAKE_CURRENT_BINARY_DIR}/shared-nuspec-metadata.xml
		@ONLY
		)
	file(READ ${CMAKE_CURRENT_BINARY_DIR}/shared-nuspec-metadata.xml SHARED_NUSPEC_METADATA)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/nuget-description.txt LINPHONESDK_DESCRIPTION)

	message(STATUS "NuGet target inferred to be Windows")
	add_subdirectory(Windows)

else() # All platforms including Windows, but not fully tested
    message(STATUS "NuGet for all platforms")
    add_subdirectory(Dotnet)
endif()
