############################################################################
# CMakeLists.txt
# Copyright (C) 2018 Belledonne Communications, Grenoble France
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

list(APPEND VOIP_SOURCE_FILES_CXX
	${CMAKE_CURRENT_SOURCE_DIR}/h26x-encoder-filter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h26x-decoder-filter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h26x-utils.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h264-nal-packer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h264-nal-unpacker.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h264-utils.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h265-nal-packer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h265-nal-unpacker.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/h265-utils.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nal-packer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/nal-unpacker.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/rfc3984.cpp
)

if (APPLE)
	list(APPEND VOIP_SOURCE_FILES_CXX
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-decoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-encoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-utils.cpp
	)
	if (ENABLE_VT_H264)
		list(APPEND VOIP_SOURCE_FILES_CXX
			${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-h264-utilities.cpp
		)
	endif()
	if(ENABLE_VT_H265 AND IOS AND (CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64"))
		list(APPEND VOIP_SOURCE_FILES_CXX
			${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-h265-utilities.cpp
		)
	endif()
endif()

if (ANDROID)
	list(APPEND VOIP_SOURCE_FILES_CXX
		${CMAKE_CURRENT_SOURCE_DIR}/media-codec-decoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/media-codec-encoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/media-codec-h264-decoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/media-codec-h264-encoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/media-codec-h265-decoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/media-codec-h265-encoder.cpp
	)
endif()

set(VOIP_SOURCE_FILES_CXX ${VOIP_SOURCE_FILES_CXX} PARENT_SCOPE)
