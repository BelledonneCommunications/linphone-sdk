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
set(DEFAULT_VALUE_ENABLE_AMR OFF)
set(DEFAULT_VALUE_ENABLE_G729 OFF)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC OFF)
set(DEFAULT_VALUE_ENABLE_ISAC OFF)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SILK OFF)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_VPX OFF)
set(DEFAULT_VALUE_ENABLE_X264 OFF)
set(DEFAULT_VALUE_ENABLE_TUNNEL OFF)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)


# Include builders
include(builders/CMakeLists.txt)


# antlr3c
set(EP_antlr3c_LINKING_TYPE "-DENABLE_STATIC=1")

# belle-sip
set(EP_bellesip_LINKING_TYPE "-DENABLE_STATIC=1")
set(EP_bellesip_EXTRA_CFLAGS "-DUSE_STRUCT_RES_STATE_NAMESERVERS ${EP_bellesip_EXTRA_CFLAGS}")

# cunit
set(EP_cunit_LINKING_TYPE "-DENABLE_STATIC=1")

# ffmpeg
set(EP_ffmpeg_LINKING_TYPE "--disable-static" "--enable-shared")

# gsm
set(EP_gsm_LINKING_TYPE "-DENABLE_STATIC=1")

# linphone
list(APPEND EP_linphone_CONFIGURE_OPTIONS
	"--disable-nls"
	"--with-readline=none"
	"--enable-gtk_ui=no"
	"--enable-console_ui=yes"
	"--disable-x11"
	"--disable-tutorials"
	"--disable-tools"
	"--disable-msg-storage"
	"--disable-video"
	"--disable-alsa"
	"--enable-relativeprefix=yes"
)
set(EP_linphone_LINKING_TYPE "--enable-static" "--disable-shared")
set(EP_linphone_PKG_CONFIG "pkg-config --static")

# mediastreamer2
list(APPEND EP_ms2_CONFIGURE_OPTIONS
	"--disable-nls"
	"--disable-theora"
	"--disable-sdl"
	"--disable-x11"
	"--disable-video"
	"--disable-alsa"
	"--enable-qsa"
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
