############################################################################
# options-uwp.cmake
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_CSHARP_WRAPPER ON)
set(DEFAULT_VALUE_ENABLE_FFMPEG OFF)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_G729 ${DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES})
set(DEFAULT_VALUE_ENABLE_G729B_CNG OFF)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_JPEG ON)
set(DEFAULT_VALUE_ENABLE_LIME ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_TOOLS OFF)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VCARD ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AEC ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AECM ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(ENABLE_NLS NO CACHE BOOL "" FORCE)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=NO")
