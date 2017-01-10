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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_FFMPEG OFF)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC ON)
set(DEFAULT_VALUE_ENABLE_ISAC ON)
set(DEFAULT_VALUE_ENABLE_JPEG ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SILK ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS OFF) #link of binary significantly slow down compilation.
set(DEFAULT_VALUE_ENABLE_VCARD ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(DEFAULT_VALUE_ENABLE_TOOLS OFF)
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

set(LINPHONE_BUILDER_CXXFLAGS "${LINPHONE_BUILDER_CXXFLAGS} -stdlib=libc++")
set(LINPHONE_BUILDER_LDFLAGS "${LINPHONE_BUILDER_LDFLAGS} -stdlib=libc++")

# Include builders
include(builders/CMakeLists.txt)

lcb_builder_cmake_options(bzrtp "-DENABLE_STRICT=NO")

# ffmpeg
lcb_builder_linking_type(ffmpeg "--enable-static" "--disable-shared" "--enable-pic")

# linphone
lcb_builder_cmake_options(linphone "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(linphone "-DENABLE_CONSOLE_UI=NO")
lcb_builder_cmake_options(linphone "-DENABLE_DAEMON=NO")
lcb_builder_cmake_options(linphone "-DENABLE_NOTIFY=NO")
lcb_builder_cmake_options(linphone "-DENABLE_TUTORIALS=NO")
lcb_builder_cmake_options(linphone "-DENABLE_UPNP=NO")
lcb_builder_cmake_options(linphone "-DENABLE_MSG_STORAGE=YES")
lcb_builder_cmake_options(linphone "-DENABLE_DOC=NO")
lcb_builder_cmake_options(linphone "-DENABLE_NLS=NO")

# mbedtls
lcb_builder_linking_type(mbedtls "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

# mediastreamer2
lcb_builder_cmake_options(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_ALSA=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_PULSEAUDIO=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_OSS=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_GLX=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_X11=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_XV=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_DOC=NO")

# opus
lcb_builder_cmake_options(opus "-DENABLE_FIXED_POINT=YES")

# ortp
lcb_builder_cmake_options(ortp "-DENABLE_DOC=NO")

# polarssl
lcb_builder_linking_type(polarssl "-DUSE_SHARED_POLARSSL_LIBRARY=0")

# speex
lcb_builder_cmake_options(speex "-DENABLE_FLOAT_API=NO")
lcb_builder_cmake_options(speex "-DENABLE_FIXED_POINT=YES")

# vpx
lcb_builder_linking_type(vpx "--enable-static" "--disable-shared")

# x264
lcb_builder_linking_type(x264 "--enable-static" "--enable-pic")
lcb_builder_install_target(x264 "install-lib-static")
