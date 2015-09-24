############################################################################
# config-windowsphone.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES OFF)
set(DEFAULT_VALUE_ENABLE_FFMPEG OFF)
set(DEFAULT_VALUE_ENABLE_ZRTP OFF)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_DTLS OFF)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC OFF)
set(DEFAULT_VALUE_ENABLE_ISAC OFF)
set(DEFAULT_VALUE_ENABLE_OPUS OFF)
set(DEFAULT_VALUE_ENABLE_SILK OFF)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AEC ON)
set(DEFAULT_VALUE_ENABLE_VPX OFF)
set(DEFAULT_VALUE_ENABLE_TUNNEL OFF)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS OFF)
set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=NO")
set(DEFAULT_VALUE_ENABLE_PACKAGING OFF)


# Global configuration
set(LINPHONE_BUILDER_CPPFLAGS "-D_ALLOW_KEYWORD_MACROS -D_CRT_SECURE_NO_WARNINGS")


# Include builders
include(builders/CMakeLists.txt)


# linphone
list(APPEND EP_linphone_CMAKE_OPTIONS
	"-DENABLE_RELATIVE_PREFIX=YES"
	"-DENABLE_CONSOLE_UI=NO"
	"-DENABLE_GTK_UI=NO"
	"-DENABLE_NOTIFY=NO"
	"-DENABLE_TOOLS=NO"
	"-DENABLE_TUTORIALS=NO"
	"-DENABLE_UNIT_TESTS=NO"
	"-DENABLE_UPNP=NO"
)

# ms2
list(APPEND EP_ms2_CMAKE_OPTIONS
	"-DENABLE_RELATIVE_PREFIX=YES"
	"-DENABLE_UNIT_TESTS=NO"
	"-DENABLE_TOOLS=NO"
)

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")
