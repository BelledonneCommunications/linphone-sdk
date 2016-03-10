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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(EP_opus_FILENAME "opus-1.0.3-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${EP_opus_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${EP_opus_FILENAME}" STATUS EP_opus_FILENAME_STATUS)
	list(GET EP_opus_FILENAME_STATUS 0 EP_opus_DOWNLOAD_STATUS)
	if(NOT EP_opus_DOWNLOAD_STATUS)
		set(EP_opus_PREBUILT 1)
	endif()
endif()

if(EP_opus_PREBUILT)
	set(EP_opus_URL "${CMAKE_CURRENT_BINARY_DIR}/${EP_opus_FILENAME}")
	set(EP_opus_BUILD_METHOD "prebuilt")
else()
	set(EP_opus_URL "http://downloads.xiph.org/releases/opus/opus-1.1.1.tar.gz")
	set(EP_opus_URL_HASH "MD5=cfb354d4c65217ca32a762f8ab15f2ac")
	set(EP_opus_EXTERNAL_SOURCE_PATHS "opus" "externals/opus")

	set(EP_opus_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/opus/CMakeLists.txt" "<SOURCE_DIR>")
	list(APPEND EP_opus_PATCH_COMMAND "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/opus/config.h.cmake" "<SOURCE_DIR>")
	set(EP_opus_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
	set(EP_opus_CMAKE_OPTIONS )
	if(ANDROID AND CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
		list(APPEND EP_opus_CMAKE_OPTIONS "-DENABLE_INTRINSICS=YES")
	endif()
endif()
