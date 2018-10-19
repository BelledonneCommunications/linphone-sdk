############################################################################
# GenerateSDK.cmake
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


# Create the zip file of the SDK
execute_process(
	COMMAND "${CMAKE_COMMAND}" "-E" "remove_directory" "linphone-sdk/apple-darwin/Tools"
	COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "linphone-sdk/apple-darwin/Tools"
	COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${LINPHONESDK_DIR}/cmake/IOS/Tools/deploy.sh" "linphone-sdk/apple-darwin/Tools"
	COMMAND "${CMAKE_COMMAND}" "-E" "remove" "-f" "linphone-sdk-ios-${LINPHONESDK_VERSION}.zip"
	COMMAND "zip" "-r" "linphone-sdk-ios-${LINPHONESDK_VERSION}.zip" "linphone-sdk/apple-darwin"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)


# Generate podspec file
configure_file("${LINPHONESDK_DIR}/cmake/IOS/linphone-sdk.podspec.cmake" "${LINPHONESDK_BUILD_DIR}/linphone-sdk.podspec" @ONLY)
