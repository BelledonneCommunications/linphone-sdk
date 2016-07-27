############################################################################
# generate_descs_header.cmake
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

set(ABS_SOURCE_FILES )
string(REPLACE " " ";" SOURCE_FILES ${SOURCE_FILES})
foreach(SOURCE_FILE ${SOURCE_FILES})
	list(APPEND ABS_SOURCE_FILES "${INPUT_DIR}/${SOURCE_FILE}")
endforeach()

execute_process(
	COMMAND ${AWK_PROGRAM} -f "${AWK_SCRIPTS_DIR}/extract-filters-names.awk" ${ABS_SOURCE_FILES}
	OUTPUT_FILE "${OUTPUT_DIR}/${TYPE}descs.txt"
)
execute_process(
	COMMAND ${AWK_PROGRAM} -f "${AWK_SCRIPTS_DIR}/define-filters.awk"
	INPUT_FILE "${OUTPUT_DIR}/${TYPE}descs.txt"
	OUTPUT_FILE "${OUTPUT_DIR}/${TYPE}descs-tmp1.h"
)
execute_process(
	COMMAND ${AWK_PROGRAM} -f "${AWK_SCRIPTS_DIR}/define-ms_${TYPE}_filter_descs.awk"
	INPUT_FILE "${OUTPUT_DIR}/${TYPE}descs.txt"
	OUTPUT_FILE "${OUTPUT_DIR}/${TYPE}descs-tmp2.h"
)
file(READ "${OUTPUT_DIR}/${TYPE}descs-tmp1.h" DESCS1)
file(READ "${OUTPUT_DIR}/${TYPE}descs-tmp2.h" DESCS2)
set(NEW_DESCS "${DESCS1}${DESCS2}")
if(EXISTS "${OUTPUT_DIR}/${TYPE}descs.h")
	file(READ "${OUTPUT_DIR}/${TYPE}descs.h" OLD_DESCS)
endif()
if(OLD_DESCS)
	if(NOT OLD_DESCS STREQUAL "${NEW_DESCS}")
		file(WRITE "${OUTPUT_DIR}/${TYPE}descs.h" "${NEW_DESCS}")
	endif()
else()
	file(WRITE "${OUTPUT_DIR}/${TYPE}descs.h" "${NEW_DESCS}")
endif()
file(REMOVE
	"${OUTPUT_DIR}/${TYPE}descs.txt"
	"${OUTPUT_DIR}/${TYPE}descs-tmp1.h"
	"${OUTPUT_DIR}/${TYPE}descs-tmp2.h"
)
