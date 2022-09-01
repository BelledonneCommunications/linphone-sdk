############################################################################
# LinphoneSdkPostquantumcryptoengineClone.cmake
# Copyright (C) 2022 Belledonne Communications, Grenoble France
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


linphone_sdk_check_git()


set(POSTQUANTUMCRYPTOENGINE_REVISION "9023b392f5e91c05e809efa218f4c1917a3f2618")


if(IS_DIRECTORY "${LINPHONESDK_DIR}/postquantumcryptoengine")
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" "pull"
		WORKING_DIRECTORY "${LINPHONESDK_DIR}/postquantumcryptoengine"
	)
else()
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" "clone" "git@gitlab.linphone.org:BC/private/postquantumcryptoengine.git" "postquantumcryptoengine"
		WORKING_DIRECTORY "${LINPHONESDK_DIR}"
	)
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" "checkout" "${POSTQUANTUMCRYPTOENGINE_REVISION}"
	WORKING_DIRECTORY "${LINPHONESDK_DIR}/postquantumcryptoengine"
)
