############################################################################
# config-bb10.cmake
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
set(DEFAULT_VALUE_ENABLE_AMRNB OFF)
set(DEFAULT_VALUE_ENABLE_AMRWB OFF)
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
get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME)
string(REGEX REPLACE "-gcc$" "" LINPHONE_BUILDER_HOST ${COMPILER_NAME})
unset(COMPILER_NAME)
set(LINPHONE_BUILDER_CPPFLAGS "-D_REENTRANT -D__QNXNTO__ -Dasm=__asm")
set(LINPHONE_BUILDER_CFLAGS "-fPIC -fstack-protector-strong")
set(LINPHONE_BUILDER_LDFLAGS "-L${QNX_TARGET}/x86/lib -Wl,-z,relro -Wl,-z,now -pie -lbps -lsocket -lslog2")
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)	# Restrict pkg-config to search in the install directory


# Include builders
include(builders/CMakeLists.txt)


# antlr3c
set(EP_antlr3c_LINKING_TYPE "-DENABLE_STATIC=YES")

# belle-sip
set(EP_bellesip_LINKING_TYPE "-DENABLE_STATIC=YES")
set(EP_bellesip_EXTRA_CFLAGS "-DUSE_STRUCT_RES_STATE_NAMESERVERS ${EP_bellesip_EXTRA_CFLAGS}")

# cunit
set(EP_cunit_LINKING_TYPE "-DENABLE_STATIC=YES")

# ffmpeg
set(EP_ffmpeg_LINKING_TYPE "--disable-static" "--enable-shared")

# gsm
set(EP_gsm_LINKING_TYPE "-DENABLE_STATIC=YES")

# linphone
set(EP_linphone_LINKING_TYPE "-DENABLE_STATIC=YES")
list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_RELATIVE_PREFIX=YES" "-DENABLE_CONSOLE_UI=NO" "-DENABLE_GTK_UI=NO" "-DENABLE_NOTIFY=NO" "-DENABLE_TOOLS=NO" "-DENABLE_TUTORIALS=NO" "-DENABLE_UNIT_TESTS=NO" "-DENABLE_UPNP=NO")

# mediastreamer2
set(EP_ms2_LINKING_TYPE "-DENABLE_STATIC=YES")
list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_RELATIVE_PREFIX=YES")
list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_QSA=YES" "-DENABLE_ALSA=YES" "-DENABLE_PULSEAUDIO=NO" "-DENABLE_OSS=NO" "-DENABLE_GLX=NO" "-DENABLE_X11=NO" "-DENABLE_XV=NO")

# opus
set(EP_opus_LINKING_TYPE "--enable-static" "--disable-shared")

# ortp
set(EP_ortp_LINKING_TYPE "--enable-static" "--disable-shared")

# polarssl
set(EP_polarssl_LINKING_TYPE "-DUSE_SHARED_POLARSSL_LIBRARY=0")

# speex
set(EP_speex_LINKING_TYPE "-DENABLE_STATIC=YES")

# srtp
set(EP_srtp_LINKING_TYPE "-DENABLE_STATIC=YES")

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared")

# xml2
set(EP_xml2_LINKING_TYPE "_DENABLE_STATIC=YES")
