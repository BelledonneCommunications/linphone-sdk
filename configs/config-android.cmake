############################################################################
# config-android.cmake
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_FFMPEG ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC ON)
set(DEFAULT_VALUE_ENABLE_ISAC ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SILK ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VCARD NO)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AEC ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(ENABLE_NLS NO CACHE BOOL "" FORCE)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=YES" "-DENABLE_SHARED=NO")
set(DEFAULT_VALUE_CMAKE_PLUGIN_LINKING_TYPE "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")


# Global configuration
set(LINPHONE_BUILDER_HOST "${CMAKE_SYSTEM_PROCESSOR}-linux-android")
set(CMAKE_INSTALL_RPATH "$ORIGIN")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi" AND ENABLE_VIDEO)
	message(STATUS "Disabling video for armv6")
	set(ENABLE_VIDEO NO CACHE BOOL "" FORCE)
	set(ENABLE_FFMPEG NO CACHE BOOL "" FORCE)
	set(ENABLE_OPENH264 NO CACHE BOOL "" FORCE)
	set(ENABLE_VPX NO CACHE BOOL "" FORCE)
	set(ENABLE_X264 NO CACHE BOOL "" FORCE)
endif()


# Include builders
include(builders/CMakeLists.txt)


# bctoolbox
set(EP_bctoolbox_LINKING_TYPE "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")
linphone_builder_add_cmake_option(bctoolbox "-DENABLE_TESTS=NO")

# belle-sip
linphone_builder_add_cmake_option(bellesip "-DENABLE_TESTS=NO")

# bzrtp
linphone_builder_add_cmake_option(bzrtp "-DENABLE_TESTS=NO")

# codec2
set(EP_codec2_EXTRA_CFLAGS "${EP_codec2_EXTRA_CFLAGS} -ffast-math")

# ffmpeg
set(EP_ffmpeg_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")

# linphone
linphone_builder_add_cmake_option(linphone "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_CONSOLE_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_GTK_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_NOTIFY=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TOOLS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TUTORIALS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_UPNP=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_MSG_STORAGE=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_DOC=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_UNIT_TESTS=YES")
set(EP_linphone_LINKING_TYPE "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")

# mbedtls
set(EP_mbedtls_LINKING_TYPE "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

# mediastreamer2
linphone_builder_add_cmake_option(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_ALSA=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_ANDROIDSND=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_PULSEAUDIO=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_OSS=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_GLX=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_V4L=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_X11=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_XV=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_TOOLS=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_DOC=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_UNIT_TESTS=NO")
set(EP_ms2_LINKING_TYPE "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")

# opus
linphone_builder_add_cmake_option(opus "-DENABLE_FIXED_POINT=YES")

# ortp
linphone_builder_add_cmake_option(ortp "-DENABLE_DOC=NO")
set(EP_ortp_LINKING_TYPE "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")

# polarssl
set(EP_polarssl_LINKING_TYPE "-DUSE_SHARED_POLARSSL_LIBRARY=0")

# speex
linphone_builder_add_cmake_option(speex "-DENABLE_FLOAT_API=NO")
linphone_builder_add_cmake_option(speex "-DENABLE_FIXED_POINT=YES")
linphone_builder_add_cmake_option(speex "-DENABLE_ARM_NEON_INTRINSICS=1")

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared")

# x264
set(EP_x264_LINKING_TYPE "--enable-static" "--enable-pic")
set(EP_x264_INSTALL_TARGET "install-lib-static")


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	# GDB server setup
	linphone_builder_apply_flags()
	linphone_builder_set_ep_directories(gdbserver)
	linphone_builder_expand_external_project_vars()
	ExternalProject_Add(TARGET_gdbserver
		DEPENDS TARGET_linphone_builder
		TMP_DIR ${ep_tmp}
		BINARY_DIR ${ep_build}
		SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/android/gdbserver"
		DOWNLOAD_COMMAND ""
		CMAKE_GENERATOR ${CMAKE_GENERATOR}
		CMAKE_ARGS ${LINPHONE_BUILDER_EP_ARGS} -DANDROID_NDK_PATH=${ANDROID_NDK_PATH} -DARCHITECTURE=${ARCHITECTURE}
	)

	# Dummy stript to not strip compiled libs from the general Makefile
	file(WRITE "${LINPHONE_BUILDER_WORK_DIR}/strip.sh" "")
else()
	# Script to be able to strip compiled libs from the general Makefile
	configure_file("${CMAKE_CURRENT_LIST_DIR}/android/strip.sh.cmake" "${LINPHONE_BUILDER_WORK_DIR}/strip.sh" @ONLY)
endif()
