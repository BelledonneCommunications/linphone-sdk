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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(EP_voamrwbenc_FILENAME "voamrwbenc-0.1.3-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${EP_voamrwbenc_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${EP_voamrwbenc_FILENAME}" STATUS EP_voamrwbenc_FILENAME_STATUS)
	list(GET EP_voamrwbenc_FILENAME_STATUS 0 EP_voamrwbenc_DOWNLOAD_STATUS)
	if(NOT EP_voamrwbenc_DOWNLOAD_STATUS)
		set(EP_voamrwbenc_PREBUILT 1)
	endif()
endif()

if(EP_voamrwbenc_PREBUILT)
	set(EP_voamrwbenc_URL "${CMAKE_CURRENT_BINARY_DIR}/${EP_voamrwbenc_FILENAME}")
	set(EP_voamrwbenc_BUILD_METHOD "prebuilt")
else()
	set(EP_voamrwbenc_URL "http://downloads.sourceforge.net/project/opencore-amr/vo-amrwbenc/vo-amrwbenc-0.1.3.tar.gz")
	set(EP_voamrwbenc_URL_HASH "MD5=f63bb92bde0b1583cb3cb344c12922e0")
	set(EP_voamrwbenc_EXTERNAL_SOURCE_PATHS "externals/vo-amrwbenc" "vo-amrwbenc")

	set(EP_voamrwbenc_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/voamrwbenc/CMakeLists.txt" "<SOURCE_DIR>")
	set(EP_voamrwbenc_CMAKE_OPTIONS )
	set(EP_voamrwbenc_LINKING_TYPE "-DENABLE_STATIC=YES")
	set(EP_voamrwbenc_DEPENDENCIES EP_opencoreamr)

	if(ANDROID)
		if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
			list(APPEND EP_voamrwbenc_CMAKE_OPTIONS "-DENABLE_ARMV7NEON=YES")
		elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi")
			list(APPEND EP_voamrwbenc_CMAKE_OPTIONS "-DENABLE_ARMV5E=YES")
		endif()
	endif()
endif()
