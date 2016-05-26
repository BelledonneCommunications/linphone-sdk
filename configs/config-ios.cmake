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
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(ENABLE_NLS NO CACHE BOOL "" FORCE)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=YES" "-DENABLE_SHARED=NO")

# Global configuration
if(NOT LINPHONE_IOS_DEPLOYMENT_TARGET)
	set(LINPHONE_IOS_DEPLOYMENT_TARGET 6.0)
endif()
set(LINPHONE_BUILDER_HOST "${CMAKE_SYSTEM_PROCESSOR}-apple-darwin")
set(COMMON_FLAGS "-miphoneos-version-min=${LINPHONE_IOS_DEPLOYMENT_TARGET} -DTARGET_OS_IPHONE=1 -D__IOS -fms-extensions")
set(LINPHONE_BUILDER_CPPFLAGS "${COMMON_FLAGS}")
set(LINPHONE_BUILDER_LDFLAGS "${COMMON_FLAGS}")
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)	# Restrict pkg-config to search in the install directory
unset(COMMON_FLAGS)
unset(LINPHONE_IOS_DEPLOYMENT_TARGET)

#XCode7 requires Cmake 3.3.20150815 at least
if(NOT ${XCODE_VERSION} VERSION_LESS 7)
	set(CMAKE_MIN_VERSION "3.3.20150815")
	if(${CMAKE_VERSION} VERSION_LESS ${CMAKE_MIN_VERSION})
		message(FATAL_ERROR "You need at least CMake version ${CMAKE_MIN_VERSION} but you are currently using ${CMAKE_VERSION}. There is no Cmake release available for it yet, so you must either compile it manually or revert to XCode6 temporary.")
	endif()
endif()

# Include builders
include(builders/CMakeLists.txt)

# bctoolbox
linphone_builder_add_cmake_option(bctoolbox "-DENABLE_TESTS=NO")

# belle-sip
linphone_builder_add_cmake_option(bellesip "-DENABLE_TESTS=NO")

# bzrtp
linphone_builder_add_cmake_option(bzrtp "-DENABLE_TESTS=NO")
linphone_builder_add_cmake_option(bzrtp "-DENABLE_STRICT=NO")

# ffmpeg
set(EP_ffmpeg_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")

# linphone
linphone_builder_add_cmake_option(linphone "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_CONSOLE_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_GTK_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_NOTIFY=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TOOLS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TUTORIALS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_UPNP=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_MSG_STORAGE=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_DOC=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_UNIT_TESTS=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_NLS=NO")

# mbedtls
set(EP_mbedtls_LINKING_TYPE "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

# mediastreamer2
linphone_builder_add_cmake_option(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_ALSA=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_PULSEAUDIO=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_OSS=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_GLX=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_X11=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_XV=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_TOOLS=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_DOC=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_UNIT_TESTS=NO")

# opus
linphone_builder_add_cmake_option(opus "-DENABLE_FIXED_POINT=YES")

# ortp
linphone_builder_add_cmake_option(ortp "-DENABLE_DOC=NO")

# polarssl
set(EP_polarssl_LINKING_TYPE "-DUSE_SHARED_POLARSSL_LIBRARY=0")

# speex
linphone_builder_add_cmake_option(speex "-DENABLE_FLOAT_API=NO")
linphone_builder_add_cmake_option(speex "-DENABLE_FIXED_POINT=YES")

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared")

# x264
set(EP_x264_LINKING_TYPE "--enable-static" "--enable-pic")
set(EP_x264_INSTALL_TARGET "install-lib-static")
