############################################################################
# ms2.cmake
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

set(EP_ms2_GIT_REPOSITORY "git://git.linphone.org/mediastreamer2.git" CACHE STRING "ms2 repository URL")
set(EP_ms2_GIT_TAG_LATEST "master" CACHE STRING "ms2 tag to use when compiling latest version")
set(EP_ms2_GIT_TAG "2.12.0" CACHE STRING "ms2 tag to use")
set(EP_ms2_EXTERNAL_SOURCE_PATHS "mediastreamer2" "linphone/mediastreamer2")
set(EP_ms2_GROUPABLE YES)

set(EP_ms2_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
set(EP_ms2_DEPENDENCIES EP_ortp EP_bctoolbox)
if(MSVC)
	set(EP_ms2_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()

set(EP_ms2_CMAKE_OPTIONS
	"-DENABLE_NON_FREE_CODECS=${ENABLE_NON_FREE_CODECS}"
	"-DENABLE_UNIT_TESTS=${ENABLE_UNIT_TESTS}"
	"-DENABLE_DEBUG_LOGS=${ENABLE_DEBUG_LOGS}"
	"-DENABLE_PCAP=${ENABLE_PCAP}"
)

# TODO: Activate strict compilation options on IOS
if(IOS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_STRICT=NO")
endif()

list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_GSM=${ENABLE_GSM}")
if(ENABLE_GSM AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_ms2_DEPENDENCIES EP_gsm)
endif()
list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_OPUS=${ENABLE_OPUS}")
if(ENABLE_OPUS AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_ms2_DEPENDENCIES EP_opus)
endif()
list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SPEEX_CODEC=${ENABLE_SPEEX}")
if(ENABLE_SPEEX AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_ms2_DEPENDENCIES EP_speex)
endif()
list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_BV16=${ENABLE_BV16}")
if(ENABLE_BV16 AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_ms2_DEPENDENCIES EP_bv16)
endif()

list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VIDEO=${ENABLE_VIDEO}")
if(ENABLE_VIDEO)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_FFMPEG=${ENABLE_FFMPEG}")
	if(ENABLE_FFMPEG AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		if(ANDROID)
			list(APPEND EP_ms2_DEPENDENCIES EP_ffmpegandroid)
		else()
			list(APPEND EP_ms2_DEPENDENCIES EP_ffmpeg)
		endif()
	endif()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VPX=${ENABLE_VPX}")
	if(ENABLE_VPX AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		list(APPEND EP_ms2_DEPENDENCIES EP_vpx)
	endif()
endif()

list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_MKV=${ENABLE_MKV}")
if(ENABLE_MKV)
	list(APPEND EP_ms2_DEPENDENCIES EP_matroska2)
endif()

list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SRTP=${ENABLE_SRTP}")
if(ENABLE_SRTP AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_ms2_DEPENDENCIES EP_srtp)
endif()
list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_ZRTP=${ENABLE_ZRTP}")
if(ENABLE_ZRTP)
	list(APPEND EP_ms2_DEPENDENCIES EP_bzrtp)
endif()
if(ENABLE_DOC)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_DOC=YES")
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_DOC=NO")
endif()
if(ENABLE_V4L AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_ms2_DEPENDENCIES EP_v4l)
endif()
