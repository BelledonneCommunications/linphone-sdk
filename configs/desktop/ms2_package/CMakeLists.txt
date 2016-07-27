############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.0)
project(MS2_PACKAGE) # Dummy project

find_package(Mediastreamer2 REQUIRED)
find_package(Git)

if(GIT_EXECUTABLE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always
		OUTPUT_VARIABLE MS2_GIT_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY ${MS2_SOURCE_DIR}
	)
else()
	set(MS2_GIT_REVISION "0.0.0")
endif()

install(DIRECTORY ${LINPHONE_OUTPUT_DIR}/
	DESTINATION .
	PATTERN bin/openh264.* EXCLUDE
)

if(WIN32)
	configure_file(WindowsSDKArchiver.cmake.in WindowsSDKArchiver.cmake @ONLY)
	install(SCRIPT ${CMAKE_BINARY_DIR}/WindowsSDKArchiver.cmake)
endif()
