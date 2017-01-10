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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Define options that are specific to the android config
lcb_add_dependent_option("Embedded OpenH264" "Embed the openh264 library instead of downloading it from Cisco." "${DEFAULT_VALUE_ENABLE_EMBEDDED_OPENH264}" "ENABLE_OPENH264" OFF)


# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_FFMPEG OFF)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC ON)
set(DEFAULT_VALUE_ENABLE_ISAC ON)
set(DEFAULT_VALUE_ENABLE_JPEG ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SILK ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VCARD ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AEC ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(DEFAULT_VALUE_ENABLE_TOOLS OFF)
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
lcb_builder_cmake_options(bctoolbox "-DENABLE_TESTS=NO")

# belle-sip
lcb_builder_cmake_options(bellesip "-DENABLE_TESTS=NO")

# bzrtp
lcb_builder_cmake_options(bzrtp "-DENABLE_TESTS=NO")

# codec2
lcb_builder_extra_cflags(codec2 "-ffast-math")

# ffmpeg
lcb_builder_linking_type(ffmpeg "--enable-static" "--disable-shared" "--enable-pic")

# linphone
lcb_builder_cmake_options(linphone "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(linphone "-DENABLE_CONSOLE_UI=NO")
lcb_builder_cmake_options(linphone "-DENABLE_DAEMON=NO")
lcb_builder_cmake_options(linphone "-DENABLE_NOTIFY=NO")
lcb_builder_cmake_options(linphone "-DENABLE_TUTORIALS=NO")
lcb_builder_cmake_options(linphone "-DENABLE_UPNP=NO")
lcb_builder_cmake_options(linphone "-DENABLE_MSG_STORAGE=YES")
lcb_builder_cmake_options(linphone "-DENABLE_DOC=NO")
lcb_builder_cmake_options(linphone "-DENABLE_UNIT_TESTS=YES")
lcb_builder_linking_type(linphone "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")

# mbedtls
lcb_builder_linking_type(mbedtls "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

# mediastreamer2
lcb_builder_cmake_options(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_ALSA=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_ANDROIDSND=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_PULSEAUDIO=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_OSS=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_GLX=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_V4L=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_X11=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_XV=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_DOC=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_UNIT_TESTS=NO")
lcb_builder_linking_type(ms2 "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")

# openh264
if(NOT ENABLE_EMBEDDED_OPENH264)
	lcb_builder_linking_type(openh264 "-shared")
endif()

# opus
lcb_builder_cmake_options(opus "-DENABLE_FIXED_POINT=YES")

# ortp
lcb_builder_cmake_options(ortp "-DENABLE_DOC=NO")
lcb_builder_linking_type(ortp "-DENABLE_STATIC=NO" "-DENABLE_SHARED=YES")

# polarssl
lcb_builder_linking_type(polarssl "-DUSE_SHARED_POLARSSL_LIBRARY=NO")

# speex
lcb_builder_cmake_options(speex "-DENABLE_FLOAT_API=NO")
lcb_builder_cmake_options(speex "-DENABLE_FIXED_POINT=YES")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
	lcb_builder_cmake_options(speex "-DENABLE_ARM_NEON_INTRINSICS=YES")
endif()

# vpx
lcb_builder_linking_type(vpx "--enable-static" "--disable-shared")

# x264
lcb_builder_linking_type(x264 "--enable-static" "--enable-pic")
lcb_builder_install_target(x264 "install-lib-static")


# Copy c++ library to install prefix
file(COPY "${ANDROID_NDK_PATH}/sources/cxx-stl/gnu-libstdc++/${GCC_VERSION}/libs/${CMAKE_SYSTEM_PROCESSOR}/libgnustl_shared.so"
	DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
)


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
