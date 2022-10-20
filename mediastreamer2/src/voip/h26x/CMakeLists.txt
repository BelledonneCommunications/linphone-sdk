############################################################################
# Copyright (c) 2010-2022 Belledonne Communications SARL.
#
# This file is part of mediastreamer2 
# (see https://gitlab.linphone.org/BC/public/mediastreamer2).
#
############################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
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

if(APPLE)
	list(APPEND VOIP_SOURCE_FILES_CXX
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-decoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-encoder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-utils.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-h264-utilities.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/videotoolbox-h265-utilities.cpp
	)
endif()

if(ANDROID)
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

