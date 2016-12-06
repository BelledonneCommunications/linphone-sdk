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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

lcb_git_repository("git://git.linphone.org/mediastreamer2.git")
lcb_git_tag_latest("master")
lcb_git_tag("2.14.0")
lcb_external_source_paths("mediastreamer2" "linphone/mediastreamer2")
lcb_groupable(YES)
lcb_spec_file("mediastreamer2.spec")
lcb_rpmbuild_name("mediastreamer")

lcb_dependencies("ortp" "bctoolbox")
if(ANDROID)
	lcb_dependencies("androidcpufeatures" "androidsupport")
endif()

lcb_cmake_options(
	"-DENABLE_NON_FREE_CODECS=${ENABLE_NON_FREE_CODECS}"
	"-DENABLE_UNIT_TESTS=${ENABLE_UNIT_TESTS}"
	"-DENABLE_DEBUG_LOGS=${ENABLE_DEBUG_LOGS}"
	"-DENABLE_PCAP=${ENABLE_PCAP}"
	"-DENABLE_DOC=${ENABLE_DOC}"
	"-DENABLE_TOOLS=${ENABLE_TOOLS}"
)

lcb_cmake_options(
	"-DENABLE_G726=${ENABLE_G726}"
	"-DENABLE_GSM=${ENABLE_GSM}"
	"-DENABLE_OPUS=${ENABLE_OPUS}"
	"-DENABLE_SPEEX_CODEC=${ENABLE_SPEEX}"
	"-DENABLE_BV16=${ENABLE_BV16}"
	"-DENABLE_G729B_CNG=${ENABLE_G729B_CNG}"
	"-DENABLE_JPEG=${ENABLE_JPEG}"
)
if(ENABLE_GSM AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	lcb_dependencies("gsm")
endif()
if(ENABLE_OPUS AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	lcb_dependencies("opus")
endif()
if(ENABLE_SPEEX AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	lcb_dependencies("speex")
endif()
if(ENABLE_BV16 AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	lcb_dependencies("bv16")
endif()
if(ENABLE_G729B_CNG)
	lcb_dependencies("bcg729bcng")
endif()
if(ENABLE_JPEG)
	lcb_dependencies("turbo-jpeg")
endif()

lcb_cmake_options("-DENABLE_VIDEO=${ENABLE_VIDEO}")
if(ENABLE_VIDEO)
	lcb_cmake_options(
		"-DENABLE_FFMPEG=${ENABLE_FFMPEG}"
		"-DENABLE_VPX=${ENABLE_VPX}"
	)
	if(ENABLE_FFMPEG AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		if(ANDROID)
			lcb_dependencies("ffmpegandroid")
		else()
			lcb_dependencies("ffmpeg")
		endif()
	endif()
	if(ENABLE_VPX AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		lcb_dependencies("vpx")
	endif()
endif()

lcb_cmake_options("-DENABLE_MKV=${ENABLE_MKV}")
if(ENABLE_MKV)
	lcb_dependencies("matroska2")
endif()

lcb_cmake_options(
	"-DENABLE_SRTP=${ENABLE_SRTP}"
	"-DENABLE_ZRTP=${ENABLE_ZRTP}"
)
if(ENABLE_SRTP AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	lcb_dependencies("srtp")
endif()
if(ENABLE_ZRTP)
	lcb_dependencies("bzrtp")
endif()

if(ENABLE_V4L AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	lcb_dependencies("v4l")
endif()

# TODO: Activate strict compilation options on IOS
if(IOS)
	lcb_cmake_options("-DENABLE_STRICT=NO")
endif()
