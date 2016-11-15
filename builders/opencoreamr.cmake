############################################################################
# opencoreamr.cmake
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

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(OPENCOREAMR_FILENAME "opencoreamr-0.1.3-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${OPENCOREAMR_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${OPENCOREAMR_FILENAME}" STATUS OPENCOREAMR_FILENAME_STATUS)
	list(GET OPENCOREAMR_FILENAME_STATUS 0 OPENCOREAMR_DOWNLOAD_STATUS)
	if(NOT OPENCOREAMR_DOWNLOAD_STATUS)
		set(OPENCOREAMR_PREBUILT 1)
	endif()
endif()

if(OPENCOREAMR_PREBUILT)
	lcb_url("${CMAKE_CURRENT_BINARY_DIR}/${OPENCOREAMR_FILENAME}")
	lcb_build_method("prebuilt")
else()
	lcb_url("http://downloads.sourceforge.net/project/opencore-amr/opencore-amr/opencore-amr-0.1.3.tar.gz")
	lcb_url_hash("MD5=09d2c5dfb43a9f6e9fec8b1ae678e725")
	lcb_external_source_paths("externals/opencore-amr" "opencore-amr")
	lcb_ignore_warnings(YES)

	lcb_patch_command("${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/opencoreamr/CMakeLists.txt" "<SOURCE_DIR>")
	lcb_linking_type("-DENABLE_STATIC=YES")

	lcb_cmake_options(
		"-DENABLE_AMRNB_DECODER=${ENABLE_AMRNB}"
		"-DENABLE_AMRNB_ENCODER=${ENABLE_AMRNB}"
		"-DENABLE_AMRWB_DECODER=${ENABLE_AMRWB}"
	)
endif()
