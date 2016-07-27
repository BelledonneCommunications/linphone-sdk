############################################################################
# CMakeLists.txt
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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
project(LINPHONE_PACKAGE) # Dummy project

#find_package(Linphone REQUIRED)
find_package(Git)

if(GIT_EXECUTABLE)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always
		OUTPUT_VARIABLE LINPHONE_GIT_REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY ${LINPHONE_SOURCE_DIR}
	)
else()
	set(LINPHONE_GIT_REVISION "0.0.0")
endif()
string(REGEX REPLACE "([0-9.]+)-?.*" "\\1" LINPHONE_VERSION "${LINPHONE_GIT_REVISION}")
string(REPLACE "." ";" SPLITTED_LINPHONE_VERSION ${LINPHONE_VERSION})
list(GET SPLITTED_LINPHONE_VERSION 0 LINPHONE_MAJOR_VERSION)
list(GET SPLITTED_LINPHONE_VERSION 1 LINPHONE_MINOR_VERSION)
list(GET SPLITTED_LINPHONE_VERSION 2 LINPHONE_MICRO_VERSION)
set(PACKAGE_VERSION "${LINPHONE_VERSION}")

# Prepare the tarball
set(LINPHONE_SDK_TMP "${CMAKE_CURRENT_BINARY_DIR}/linphone-raspberry")
file(REMOVE_RECURSE "${LINPHONE_SDK_TMP}")
file(COPY "${LINPHONE_OUTPUT_DIR}" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/desktop-raspberry" "${LINPHONE_SDK_TMP}")
file(REMOVE_RECURSE "${LINPHONE_SDK_TMP}/etc")
file(REMOVE_RECURSE "${LINPHONE_SDK_TMP}/sbin")
file(REMOVE_RECURSE "${LINPHONE_SDK_TMP}/share/doc")
file(REMOVE_RECURSE "${LINPHONE_SDK_TMP}/share/java")
file(REMOVE_RECURSE "${LINPHONE_SDK_TMP}/share/man")
file(GLOB FILES_TO_DELETE
	"${LINPHONE_SDK_TMP}/bin/cx18*"
	"${LINPHONE_SDK_TMP}/bin/decode_tm*"
	"${LINPHONE_SDK_TMP}/bin/dvb*"
	"${LINPHONE_SDK_TMP}/bin/ir-*"
	"${LINPHONE_SDK_TMP}/bin/ivtv*"
	"${LINPHONE_SDK_TMP}/bin/rds-*"
	"${LINPHONE_SDK_TMP}/bin/v4l*"
)
file(REMOVE ${FILES_TO_DELETE})

# Generate the tarball containing the output of the build
add_custom_target(create_tar ALL COMMAND
	${CMAKE_COMMAND} -E tar "cvjf" "linphone-raspberry-pi${RASPBERRY_VERSION}-${LINPHONE_GIT_REVISION}.tar.bz2" "${LINPHONE_SDK_TMP}"
)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/linphone-raspberry-pi${RASPBERRY_VERSION}-${LINPHONE_GIT_REVISION}.tar.bz2" DESTINATION .)
