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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Define default values for the linphone builder options
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(DEFAULT_VALUE_ENABLE_TOOLS OFF)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=YES" "-DENABLE_SHARED=NO")

# Global configuration
get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME)
string(REGEX REPLACE "-gcc$" "" LINPHONE_BUILDER_HOST ${COMPILER_NAME})
unset(COMPILER_NAME)
set(LINPHONE_BUILDER_CPPFLAGS "-D_REENTRANT -D__QNXNTO__ -Dasm=__asm")
set(LINPHONE_BUILDER_CFLAGS "-fPIC -fstack-protector-strong")
set(LINPHONE_BUILDER_LDFLAGS "-Wl,-z,relro -Wl,-z,now -pie -lbps -lsocket -lslog2")
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)	# Restrict pkg-config to search in the install directory

# Include builders
include(builders/CMakeLists.txt)

# bctoolbox
lcb_builder_cmake_options(bctoolbox "-DENABLE_TESTS=NO")

# belle-sip
set(EP_bellesip_EXTRA_CFLAGS "-DUSE_STRUCT_RES_STATE_NAMESERVERS ${EP_bellesip_EXTRA_CFLAGS}")
lcb_builder_cmake_options(bellesip "-DENABLE_TESTS=NO")

# bzrtp
lcb_builder_cmake_options(bzrtp "-DENABLE_TESTS=NO")

# linphone
lcb_builder_cmake_options(linphone "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(linphone "-DENABLE_CONSOLE_UI=NO")
lcb_builder_cmake_options(linphone "-DENABLE_DAEMON=NO")
lcb_builder_cmake_options(linphone "-DENABLE_NOTIFY=NO")
lcb_builder_cmake_options(linphone "-DENABLE_TUTORIALS=NO")
lcb_builder_cmake_options(linphone "-DENABLE_UNIT_TESTS=NO")
lcb_builder_cmake_options(linphone "-DENABLE_UPNP=NO")
lcb_builder_cmake_options(linphone "-DENABLE_MSG_STORAGE=YES")
lcb_builder_cmake_options(linphone "-DENABLE_NLS=NO")
lcb_builder_cmake_options(linphone "-DENABLE_CALL_LOGS_STORAGE=YES")

# mbedtls
set(EP_mbedtls_LINKING_TYPE "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

# mediastreamer2
lcb_builder_cmake_options(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_QSA=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_ALSA=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_PULSEAUDIO=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_OSS=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_GLX=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_X11=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_XV=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_MKV=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_QNX=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_V4L=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_UNIT_TESTS=NO")

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared")
