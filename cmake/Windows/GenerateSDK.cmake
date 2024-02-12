# ###########################################################################
# GenerateSDK.cmake
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
#
# ###########################################################################
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
# ###########################################################################

include("${LINPHONESDK_DIR}/cmake/LinphoneSdkUtils.cmake")

find_program(7Z_PROGRAM 7z.exe)

if(7Z_PROGRAM)
	if(NOT ENABLE_EMBEDDED_OPENH264)
		set(7Z_MORE_CONFIG "-xr!openh264.dll")
	else()
		set(7Z_MORE_CONFIG "")
	endif()

	execute_process(
		COMMAND ${7Z_PROGRAM} "a" "-r" "linphone-sdk-${LINPHONESDK_WINDOWS_ARCH}-${LINPHONESDK_VERSION}.zip" "${LINPHONESDK_FOLDER}" ${7Z_MORE_CONFIG}
		WORKING_DIRECTORY "${LINPHONESDK_BUILD_DIR}"
	)
else()
	message(WARNING "7z has not been found, cannot generate the SDK!")
endif()
