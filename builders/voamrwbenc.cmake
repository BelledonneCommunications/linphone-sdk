############################################################################
# voamrwbenc.cmake
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
	set(VOAMRWBENC_FILENAME "voamrwbenc-0.1.3-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${VOAMRWBENC_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${VOAMRWBENC_FILENAME}" STATUS VOAMRWBENC_FILENAME_STATUS)
	list(GET VOAMRWBENC_FILENAME_STATUS 0 VOAMRWBENC_DOWNLOAD_STATUS)
	if(NOT VOAMRWBENC_DOWNLOAD_STATUS)
		set(VOAMRWBENC_PREBUILT 1)
	endif()
endif()

if(VOAMRWBENC_PREBUILT)
	lcb_url("${CMAKE_CURRENT_BINARY_DIR}/${VOAMRWBENC_FILENAME}")
	lcb_build_method("prebuilt")
else()
	lcb_url("http://downloads.sourceforge.net/project/opencore-amr/vo-amrwbenc/vo-amrwbenc-0.1.3.tar.gz")
	lcb_url_hash("MD5=f63bb92bde0b1583cb3cb344c12922e0")
	lcb_external_source_paths("externals/vo-amrwbenc" "vo-amrwbenc")
	lcb_ignore_warnings(YES)

	lcb_dependencies("opencoreamr")

	lcb_patch_command("${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/voamrwbenc/CMakeLists.txt" "<SOURCE_DIR>")
	lcb_linking_type("-DENABLE_STATIC=YES")

	if(ANDROID)
		if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
			lcb_cmake_options("-DENABLE_ARMV7NEON=YES")
		elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi")
			lcb_cmake_options("-DENABLE_ARMV5E=YES")
		endif()
	endif()
endif()
