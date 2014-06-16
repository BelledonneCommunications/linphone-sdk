############################################################################
# config-ios.cmake
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

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_VIDEO OFF)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_FFMPEG OFF)
set(DEFAULT_VALUE_ENABLE_ZRTP OFF)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_AMRNB ON)
set(DEFAULT_VALUE_ENABLE_AMRWB ON)
set(DEFAULT_VALUE_ENABLE_G729 OFF)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC OFF)
set(DEFAULT_VALUE_ENABLE_ISAC OFF)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SILK OFF)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AEC OFF)
set(DEFAULT_VALUE_ENABLE_H263 OFF)
set(DEFAULT_VALUE_ENABLE_H263P OFF)
set(DEFAULT_VALUE_ENABLE_MPEG4 OFF)
set(DEFAULT_VALUE_ENABLE_OPENH264 OFF)
set(DEFAULT_VALUE_ENABLE_VPX OFF)
set(DEFAULT_VALUE_ENABLE_X264 OFF)
set(DEFAULT_VALUE_ENABLE_TUNNEL OFF)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)


# Global configuration
set(SDK_VERSION 4.0)
get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME)
string(REGEX REPLACE "-clang$" "" LINPHONE_BUILDER_HOST ${COMPILER_NAME})
unset(COMPILER_NAME)
if("${PLATFORM}" MATCHES "Simulator")
	set(CLANG_TARGET_SPECIFIER "ios-simulator-version-min")
else("${PLATFORM}" MATCHES "Simulator")
	set(CLANG_TARGET_SPECIFIER "iphoneos-version-min")
endif("${PLATFORM}" MATCHES "Simulator")
list(get CMAKE_FIND_ROOT_PATH 0 SYSROOT_PATH)
set(COMMON_FLAGS "-arch ${CMAKE_SYSTEM_PROCESSOR} -isysroot ${SYSROOT_PATH} -m${CLANG_TARGET_SPECIFIER}=${SDK_VERSION} -DTARGET_OS_IPHONE=1 -D__IOS -fms-extensions")
set(LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS "${COMMON_FLAGS} -Dasm=__asm")
set(LINPHONE_BUILDER_TOOLCHAIN_CFLAGS "-std=c99")
set(LINPHONE_BUILDER_TOOLCHAIN_OBJCFLAGS "-std=c99 ${COMMON_FLAGS} -x objective-c -fexceptions -gdwarf-2 -fobjc-abi-version=2 -fobjc-legacy-dispatch")
set(LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS "${COMMON_FLAGS}")
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)	# Restrict pkg-config to search in the install directory
unset(COMMON_FLAGS)
unset(CLANG_TARGET_SPECIFIER)
unset(SYSROOT_PATH)
unset(SDK_VERSION)


# Include builders
include(builders/CMakeLists.txt)


# antlr3c
set(EP_antlr3c_LINKING_TYPE "-DENABLE_STATIC=1")

# belle-sip
set(EP_bellesip_LINKING_TYPE "-DENABLE_STATIC=1")

# cunit
set(EP_cunit_LINKING_TYPE "-DENABLE_STATIC=1")

# ffmpeg
set(EP_ffmpeg_LINKING_TYPE "--enable-static" "--disable-shared")

# gsm
set(EP_gsm_LINKING_TYPE "-DENABLE_STATIC=1")

# linphone
list(APPEND EP_linphone_CONFIGURE_OPTIONS
	"--disable-nls"
	"--with-readline=none"
	"--enable-gtk_ui=no"
	"--enable-console_ui=no"
	"--disable-x11"
	"--disable-tutorials"
	"--disable-tools"
	"--disable-msg-storage"
	"--disable-video"
	"--disable-alsa"
	"--enable-relativeprefix=yes"
	"--disable-tests"
)
set(EP_linphone_LINKING_TYPE "--enable-static" "--disable-shared")

# mediastreamer2
list(APPEND EP_ms2_CONFIGURE_OPTIONS
	"--disable-nls"
	"--disable-theora"
	"--disable-sdl"
	"--disable-x11"
	"--disable-video"
	"--disable-alsa"
	"--enable-relativeprefix=yes"
)
set(EP_ms2_LINKING_TYPE "--enable-static" "--disable-shared")

# opus
set(EP_opus_LINKING_TYPE "--enable-static" "--disable-shared")

# ortp
set(EP_ortp_LINKING_TYPE "--enable-static" "--disable-shared")

# polarssl
set(EP_polarssl_LINKING_TYPE "-DUSE_SHARED_POLARSSL_LIBRARY=0")

# speex
set(EP_speex_LINKING_TYPE "-DENABLE_STATIC=1")

# srtp
set(EP_srtp_LINKING_TYPE "-DENABLE_STATIC=1")

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared")

# xml2
set(EP_xml2_LINKING_TYPE "--enable-static" "--disable-shared")
