############################################################################
# LinphoneSdkTunnelClone.cmake
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


linphone_sdk_check_git()


set(TUNNEL_REVISION "23a8567ad0349049d380294f52f525c8cb000826")


if(IS_DIRECTORY "${LINPHONESDK_DIR}/tunnel")
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" "pull"
		WORKING_DIRECTORY "${LINPHONESDK_DIR}/tunnel"
	)
else()
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" "clone" "git@gitlab.linphone.org:BC/private/tunnel.git" "tunnel"
		WORKING_DIRECTORY "${LINPHONESDK_DIR}"
	)
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" "checkout" "${TUNNEL_REVISION}"
	WORKING_DIRECTORY "${LINPHONESDK_DIR}/tunnel"
)
