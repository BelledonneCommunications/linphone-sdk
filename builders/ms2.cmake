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
if(LINPHONE_BUILDER_LATEST)
	set(EP_ms2_GIT_TAG "master")
else()
	set(EP_ms2_GIT_TAG "974f7ae2c810a4662d4c1b6a644ee9d0be31e894")
endif()

if(MSVC)
	set(EP_ms2_CMAKE_OPTIONS )
	set(EP_ms2_LINKING_TYPE "-DENABLE_STATIC=NO")
	set(EP_ms2_DEPENDENCIES EP_ortp)
	if(ENABLE_GSM)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_GSM=YES")
		list(APPEND EP_ms2_DEPENDENCIES EP_gsm)
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_GSM=NO")
	endif()
	if(ENABLE_OPUS)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_OPUS=YES")
		list(APPEND EP_ms2_DEPENDENCIES EP_opus)
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_OPUS=NO")
	endif()
	if(ENABLE_SPEEX)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SPEEX=YES")
		list(APPEND EP_ms2_DEPENDENCIES EP_speex)
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SPEEX=NO")
	endif()
	if(ENABLE_VIDEO)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VIDEO=YES")
		if(ENABLE_FFMPEG)
			list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_FFMPEG=YES")
			list(APPEND EP_ms2_DEPENDENCIES EP_ffmpeg)
		else()
			list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_FFMPEG=NO")
		endif()
		if(ENABLE_VPX)
			list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VPX=YES")
			list(APPEND EP_ms2_DEPENDENCIES EP_vpx)
		else()
			list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VPX=NO")
		endif()
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VIDEO=NO")
	endif()
	set(EP_ms2_EXTRA_LDFLAGS "/SAFESEH:NO")
else()
	set(EP_ms2_BUILD_METHOD "autotools")
	set(EP_ms2_USE_AUTOGEN "yes")
	set(EP_ms2_CONFIG_H_FILE mediastreamer-config.h)
	set(EP_ms2_CROSS_COMPILATION_OPTIONS
		"--prefix=${CMAKE_INSTALL_PREFIX}"
		"--host=${LINPHONE_BUILDER_HOST}"
	)
	set(EP_ms2_CONFIGURE_OPTIONS
		"--disable-strict"
		"--enable-external-ortp"
		"--disable-theora"
	)
	set(EP_ms2_LINKING_TYPE "--disable-static" "--enable-shared")
	set(EP_ms2_DEPENDENCIES EP_ortp)

	if(ENABLE_GSM)
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--with-gsm=${CMAKE_INSTALL_PREFIX}")
		list(APPEND EP_ms2_DEPENDENCIES EP_gsm)
	else()
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--disable-gsm")
	endif()

	if(ENABLE_OPUS)
		list(APPEND EP_ms2_DEPENDENCIES EP_opus)
	else()
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--disable-opus")
	endif()

	if(ENABLE_SPEEX)
		list(APPEND EP_ms2_DEPENDENCIES EP_speex)
	else()
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--disable-speex")
	endif()

	if(ENABLE_VIDEO)
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--enable-video")
	else()
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--disable-video")
	endif()

	if(ENABLE_FFMPEG)
		list(APPEND EP_ms2_DEPENDENCIES EP_ffmpeg)
	else()
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--disable-ffmpeg")
	endif()

	if(ENABLE_VPX)
		list(APPEND EP_ms2_DEPENDENCIES EP_vpx)
	else()
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--disable-vp8")
	endif()

	if("${BUILD_V4L}" STREQUAL "yes")
		list(APPEND EP_ms2_DEPENDENCIES EP_v4l)
	endif()

	if(ENABLE_UNIT_TESTS)
		list(APPEND EP_ms2_DEPENDENCIES EP_cunit)
	else()
		list(APPEND EP_ms2_CONFIGURE_OPTIONS "--disable-tests")
	endif()
endif()
