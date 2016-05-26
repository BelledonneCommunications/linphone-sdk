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
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)

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
linphone_builder_add_cmake_option(bctoolbox "-DENABLE_TESTS=NO")

# belle-sip
set(EP_bellesip_EXTRA_CFLAGS "-DUSE_STRUCT_RES_STATE_NAMESERVERS ${EP_bellesip_EXTRA_CFLAGS}")
linphone_builder_add_cmake_option(bellesip "-DENABLE_TESTS=NO")

# bzrtp
linphone_builder_add_cmake_option(bzrtp "-DENABLE_TESTS=NO")

# linphone
linphone_builder_add_cmake_option(linphone "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_CONSOLE_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_GTK_UI=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_NOTIFY=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TOOLS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_TUTORIALS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_UNIT_TESTS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_UPNP=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_MSG_STORAGE=YES")
linphone_builder_add_cmake_option(linphone "-DENABLE_NLS=NO")
linphone_builder_add_cmake_option(linphone "-DENABLE_CALL_LOGS_STORAGE=YES")

# mbedtls
set(EP_mbedtls_LINKING_TYPE "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

# mediastreamer2
linphone_builder_add_cmake_option(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_QSA=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_ALSA=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_PULSEAUDIO=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_OSS=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_GLX=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_X11=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_XV=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_MKV=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_QNX=YES")
linphone_builder_add_cmake_option(ms2 "-DENABLE_V4L=NO")
linphone_builder_add_cmake_option(ms2 "-DENABLE_UNIT_TESTS=NO")

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared")
