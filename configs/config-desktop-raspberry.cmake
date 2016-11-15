############################################################################
# config-desktop-raspberry.cmake
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

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_BV16 ON)
set(DEFAULT_VALUE_ENABLE_FFMPEG ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_GTK_UI OFF)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VCARD ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI OFF)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)


message(STATUS "Buiding for Raspberry PI ${RASPBERRY_VERSION}")

get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME)
string(REGEX REPLACE "-gcc$" "" LINPHONE_BUILDER_HOST ${COMPILER_NAME})
unset(COMPILER_NAME)

set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/:${SYSROOT_PATH}/usr/lib/pkgconfig")

include("configs/config-desktop-common.cmake")

# ffmpeg
lcb_builder_cross_compilation_options(ffmpeg
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--enable-cross-compile"
	"--cross-prefix=arm-linux-gnueabihf-"
	"--arch=arm"
	"--target-os=linux"
)

# opus
lcb_builder_cmake_options(opus "-DENABLE_FIXED_POINT=YES")

# speex
lcb_builder_cmake_options(speex
	"-DENABLE_FLOAT_API=NO"
	"-DENABLE_FIXED_POINT=YES"
)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
	lcb_builder_cmake_options(speex "-DENABLE_ARM_NEON_INTRINSICS=YES")
endif()

# vpx
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
	lcb_builder_cross_compilation_options(vpx
		"--prefix=${CMAKE_INSTALL_PREFIX}"
		"--target=armv7-linux-gcc"
	)
else()
	lcb_builder_cross_compilation_options(vpx
		"--prefix=${CMAKE_INSTALL_PREFIX}"
		"--target=armv6-linux-gcc"
	)
endif()

# Add config step for packaging
set(LINPHONE_BUILDER_ADDITIONAL_CONFIG_STEPS "${CMAKE_CURRENT_LIST_DIR}/desktop-raspberry/additional_steps.cmake")
