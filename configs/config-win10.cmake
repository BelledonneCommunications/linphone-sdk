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
set(DEFAULT_VALUE_ENABLE_DTLS ON)
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
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI ON)
set(DEFAULT_VALUE_ENABLE_WEBRTC_AEC ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(ENABLE_NLS NO CACHE BOOL "" FORCE)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=NO")


# Global configuration
set(LINPHONE_BUILDER_CPPFLAGS "-D_ALLOW_KEYWORD_MACROS -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS")


# Include builders
include(builders/CMakeLists.txt)


# linphone
linphone_builder_add_cmake_option(linphone "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_CONSOLE_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_GTK_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_NOTIFY=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TOOLS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TUTORIALS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_UPNP=NO")

# mbedtls
set(EP_mbedtls_EXTRA_CFLAGS "${EP_mbedtls_EXTRA_CFLAGS} -DMBEDTLS_NO_PLATFORM_ENTROPY")

# ms2
linphone_builder_add_cmake_option(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_TOOLS=NO")

# opus
linphone_builder_add_cmake_option(opus "-DENABLE_ASM=NO")
linphone_builder_add_cmake_option(opus "-DENABLE_FIXED_POINT=YES")
set(EP_opus_LINKING_TYPE "-DENABLE_STATIC=YES")


if(LINPHONE_BUILDER_TARGET STREQUAL linphone)
	# Build liblinphone C# wrapper
	linphone_builder_apply_flags()
	linphone_builder_set_ep_directories(linphone_wrapper)
	linphone_builder_expand_external_project_vars()
	ExternalProject_Add(TARGET_linphone_wrapper
		DEPENDS TARGET_linphone_builder
		TMP_DIR ${ep_tmp}
		BINARY_DIR ${ep_build}
		SOURCE_DIR "${LINPHONE_BUILDER_EXTERNAL_SOURCE_PATH}/../Native"
		DOWNLOAD_COMMAND ""
		CMAKE_GENERATOR ${CMAKE_GENERATOR}
		CMAKE_ARGS ${LINPHONE_BUILDER_EP_ARGS}
	)
endif()
