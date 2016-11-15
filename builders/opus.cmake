############################################################################
# opus.cmake
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
	set(OPUS_FILENAME "opus-1.0.3-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${OPUS_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${OPUS_FILENAME}" STATUS OPUS_FILENAME_STATUS)
	list(GET OPUS_FILENAME_STATUS 0 OPUS_DOWNLOAD_STATUS)
	if(NOT OPUS_DOWNLOAD_STATUS)
		set(OPUS_PREBUILT 1)
	endif()
endif()

if(OPUS_PREBUILT)
	lcb_url("${CMAKE_CURRENT_BINARY_DIR}/${OPUS_FILENAME}")
	lcb_build_method("prebuilt")
else()
	lcb_url("http://downloads.xiph.org/releases/opus/opus-1.1.1.tar.gz")
	lcb_url_hash("MD5=cfb354d4c65217ca32a762f8ab15f2ac")
	lcb_external_source_paths("opus" "externals/opus")
	lcb_may_be_found_on_system(YES)
	lcb_ignore_warnings(YES)

	lcb_patch_command(
		"${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/opus/CMakeLists.txt" "<SOURCE_DIR>"
		"COMMAND"
		"${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/opus/config.h.cmake" "<SOURCE_DIR>"
	)
	if(ANDROID AND CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
		lcb_cmake_options("-DENABLE_INTRINSICS=YES")
	endif()
endif()
