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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(EP_opencoreamr_FILENAME "opencoreamr-0.1.3-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${EP_opencoreamr_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${EP_opencoreamr_FILENAME}" STATUS EP_opencoreamr_FILENAME_STATUS)
	list(GET EP_opencoreamr_FILENAME_STATUS 0 EP_opencoreamr_DOWNLOAD_STATUS)
	if(NOT EP_opencoreamr_DOWNLOAD_STATUS)
		set(EP_opencoreamr_PREBUILT 1)
	endif()
endif()

if(EP_opencoreamr_PREBUILT)
	set(EP_opencoreamr_URL "${CMAKE_CURRENT_BINARY_DIR}/${EP_opencoreamr_FILENAME}")
	set(EP_opencoreamr_BUILD_METHOD "prebuilt")
else()
	set(EP_opencoreamr_URL "http://downloads.sourceforge.net/project/opencore-amr/opencore-amr/opencore-amr-0.1.3.tar.gz")
	set(EP_opencoreamr_URL_HASH "MD5=09d2c5dfb43a9f6e9fec8b1ae678e725")
	set(EP_opencoreamr_EXTERNAL_SOURCE_PATHS  "externals/opencore-amr" "opencore-amr")

	set(EP_opencoreamr_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/opencoreamr/CMakeLists.txt" "<SOURCE_DIR>")
	set(EP_opencoreamr_LINKING_TYPE "-DENABLE_STATIC=YES")

	set(EP_opencoreamr_CMAKE_OPTIONS
		"-DENABLE_AMRNB_DECODER=${ENABLE_AMRNB}"
		"-DENABLE_AMRNB_ENCODER=${ENABLE_AMRNB}"
		"-DENABLE_AMRWB_DECODER=${ENABLE_AMRWB}"
	)
endif()
