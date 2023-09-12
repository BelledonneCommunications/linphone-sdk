############################################################################
# CleanSDK.cmake
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
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

include("${LINPHONESDK_DIR}/cmake/LinphoneSdkUtils.cmake")


# Delete previously created zip files of the SDK
file(GLOB _ZIP_FILES LIST_DIRECTORIES false "*.zip")
foreach(_FILE IN LISTS _ZIP_FILES)
	execute_process(
		COMMAND "${CMAKE_COMMAND}" "-E" "remove" "-f" "${_FILE}"
	)
endforeach()
