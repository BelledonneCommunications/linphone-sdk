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


# belle-sip
set(EP_bellesip_LINKING_TYPE "-DENABLE_STATIC=1")

# ortp
set(EP_ortp_LINKING_TYPE "--enable-static" "--disable-shared")

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

