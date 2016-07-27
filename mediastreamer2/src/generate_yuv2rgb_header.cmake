############################################################################
# generate_yuv2rgb_header.cmake
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

execute_process(
	COMMAND "${PYTHON_EXECUTABLE}" "${INPUT_DIR}/../tools/xxd.py" "-i" "${SOURCE_FILE}"
	OUTPUT_FILE "${OUTPUT_DIR}/${SOURCE_FILE}.tmp"
	WORKING_DIRECTORY ${INPUT_DIR}
)
execute_process(
	COMMAND "${SED_PROGRAM}" "s/}\;/,0x00}\;/" "${OUTPUT_DIR}/${SOURCE_FILE}.tmp"
	OUTPUT_FILE "${OUTPUT_DIR}/${SOURCE_FILE}.h.tmp"
)
if(EXISTS "${OUTPUT_DIR}/${SOURCE_FILE}.h")
	file(READ "${OUTPUT_DIR}/${SOURCE_FILE}.h" OLD_CONTENT)
	file(READ "${OUTPUT_DIR}/${SOURCE_FILE}.h.tmp" NEW_CONTENT)
	if(NOT NEW_CONTENT STREQUAL OLD_CONTENT)
		file(RENAME "${OUTPUT_DIR}/${SOURCE_FILE}.h.tmp" "${OUTPUT_DIR}/${SOURCE_FILE}.h")
	endif()
else()
	file(RENAME "${OUTPUT_DIR}/${SOURCE_FILE}.h.tmp" "${OUTPUT_DIR}/${SOURCE_FILE}.h")
endif()
file(REMOVE
	"${OUTPUT_DIR}/${SOURCE_FILE}.tmp"
	"${OUTPUT_DIR}/${SOURCE_FILE}.h.tmp"
)
