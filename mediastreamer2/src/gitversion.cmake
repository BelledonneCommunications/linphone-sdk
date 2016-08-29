############################################################################
# gitversion.cmake
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

if(GIT_EXECUTABLE)
	macro(GIT_COMMAND OUTPUT_VAR)
		set(GIT_ARGS ${ARGN})
		execute_process(
			COMMAND ${GIT_EXECUTABLE} ${ARGN}
			WORKING_DIRECTORY ${WORK_DIR}
			OUTPUT_VARIABLE ${OUTPUT_VAR}
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
	endmacro()

	GIT_COMMAND(GIT_DESCRIBE describe --always)
	GIT_COMMAND(GIT_TAG describe --abbrev=0)
	GIT_COMMAND(GIT_REVISION rev-parse HEAD)
endif()

if(GIT_DESCRIBE)
	if(NOT GIT_TAG STREQUAL MEDIASTREAMER_VERSION)
		message(FATAL_ERROR "MEDIASTREAMER_VERSION (${MEDIASTREAMER_VERSION}) and git tag (${GIT_TAG}) differ. Please put them identical")
	endif()
	set(GIT_VERSION "${GIT_DESCRIBE}")
elseif(GIT_REVISION)
	set(GIT_VERSION "${MEDIASTREAMER_VERSION}_${GIT_REVISION}")
else()
	set(GIT_VERSION "unknown")
endif()
configure_file("${WORK_DIR}/gitversion.h.in" "${OUTPUT_DIR}/gitversion.h" @ONLY)
