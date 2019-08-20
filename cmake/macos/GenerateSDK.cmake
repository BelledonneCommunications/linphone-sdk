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


list(APPEND CMAKE_MODULE_PATH "${LINPHONESDK_DIR}/cmake")
include(LinphoneSdkUtils)


# Create the zip file of the SDK
execute_process(
	COMMAND "zip" "-r" "--symlinks" "linphone-sdk-${LINPHONESDK_VERSION}.zip" "linphone-sdk/desktop"
	WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
)

# Generate podspec file
file(READ "${LINPHONESDK_DIR}/LICENSE.txt" LINPHONESDK_LICENSE)
file(READ "${LINPHONESDK_ENABLED_FEATURES_FILENAME}" LINPHONESDK_ENABLED_FEATURES)
configure_file("${LINPHONESDK_DIR}/cmake/macos/linphone-sdk.podspec.cmake" "${LINPHONESDK_BUILD_DIR}/linphone-sdk.podspec" @ONLY)
