############################################################################
# EktServer.cmake
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

include("${PROJECT_SOURCE_DIR}/cmake/LinphoneSdkUtils.cmake")

linphone_sdk_check_git()

set(EKT_SERVER_REVISION "44a5d89c75b0fe61a5d070a70027d814be73b94b")

if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/ekt-server")
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" "fetch" "--all"
		WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/ekt-server"
	)
else()
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" "clone" "git@gitlab.linphone.org:BC/private/ekt-server.git" "ekt-server"
		WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	)
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" "checkout" "${EKT_SERVER_REVISION}"
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/ekt-server"
)
