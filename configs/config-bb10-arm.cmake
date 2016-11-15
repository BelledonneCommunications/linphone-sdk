############################################################################
# config-bb10-arm.cmake
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

set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)

include(configs/config-bb10.cmake)

# Global configuration
set(LINPHONE_BUILDER_CFLAGS "${LINPHONE_BUILDER_CFLAGS} -march=armv7-a -mfpu=neon")


# speex
lcb_builder_cmake_options(speex "-DENABLE_FLOAT_API=0")
lcb_builder_cmake_options(speex "-DENABLE_FIXED_POINT=1")
lcb_builder_cmake_options(speex "-DENABLE_ARM_NEON_INTRINSICS=1")

# opus
list(APPEND EP_opus_CONFIGURE_OPTIONS "--enable-fixed-point")

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")
