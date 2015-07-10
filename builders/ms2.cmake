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

set(EP_ms2_GIT_REPOSITORY "git://git.linphone.org/mediastreamer2.git")
set(EP_ms2_GIT_TAG_LATEST "master")
set(EP_ms2_GIT_TAG "810a99275823edc1ca8567c892406c016c48d2f8")
set(EP_ms2_EXTERNAL_SOURCE_PATHS "mediastreamer2" "linphone/mediastreamer2")

set(EP_ms2_CMAKE_OPTIONS )
set(EP_ms2_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
set(EP_ms2_DEPENDENCIES EP_ortp)
if(ENABLE_NON_FREE_CODECS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_NON_FREE_CODECS=YES")
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_NON_FREE_CODECS=NO")
endif()
if(ENABLE_GSM)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_GSM=YES")
	if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		list(APPEND EP_ms2_DEPENDENCIES EP_gsm)
	endif()
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_GSM=NO")
endif()
if(ENABLE_OPUS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_OPUS=YES")
	if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		list(APPEND EP_ms2_DEPENDENCIES EP_opus)
	endif()
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_OPUS=NO")
endif()
if(ENABLE_SPEEX)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SPEEX=YES")
	if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		list(APPEND EP_ms2_DEPENDENCIES EP_speex)
	endif()
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SPEEX=NO")
endif()
if(ENABLE_VIDEO)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VIDEO=YES")
	if(ENABLE_FFMPEG)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_FFMPEG=YES")
		if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
			list(APPEND EP_ms2_DEPENDENCIES EP_ffmpeg)
		endif()
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_FFMPEG=NO")
	endif()
	if(ENABLE_VPX)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VPX=YES")
		if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
			list(APPEND EP_ms2_DEPENDENCIES EP_vpx)
		endif()
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VPX=NO")
	endif()
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VIDEO=NO")
endif()
if(ENABLE_SRTP)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SRTP=YES")
	if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		list(APPEND EP_ms2_DEPENDENCIES EP_srtp)
	endif()
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SRTP=NO")
endif()
if(ENABLE_ZRTP)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_ZRTP=YES")
	list(APPEND EP_ms2_DEPENDENCIES EP_bzrtp)
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_ZRTP=NO")
endif()
if(ENABLE_DTLS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_DTLS=YES")
	list(APPEND EP_ms2_DEPENDENCIES EP_polarssl)
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_DTLS=NO")
endif()
if(ENABLE_UNIT_TESTS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=YES")
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=NO")
endif()
if(ENABLE_DEBUG_LOGS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_DEBUG_LOGS=YES")
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_DEBUG_LOGS=NO")
endif()
if(ENABLE_V4L AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_ms2_DEPENDENCIES EP_v4l)
endif()
if(MSVC)
	set(EP_ms2_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
