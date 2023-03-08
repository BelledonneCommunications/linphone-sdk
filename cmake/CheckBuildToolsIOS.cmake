############################################################################
# CheckBuildToolsIOS.cmake
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

if(NOT APPLE)
	message(FATAL_ERROR "You need to use a Mac OS X system to build for iOS")
endif()

execute_process(
	COMMAND "xcrun" "--sdk" "iphoneos" "--show-sdk-path"
	RESULT_VARIABLE _xcrun_status
)
if(_xcrun_status)
	message(FATAL_ERROR "iOS SDK not found, please install Xcode from AppStore or equivalent")
endif()


include("${CMAKE_CURRENT_LIST_DIR}/CheckBuildToolsCommon.cmake")
