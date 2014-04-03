############################################################################
# config-desktop.cmake
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
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_FFMPEG ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_AMRNB ON)
set(DEFAULT_VALUE_ENABLE_AMRWB ON)
set(DEFAULT_VALUE_ENABLE_G729 ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_ILBC ON)
set(DEFAULT_VALUE_ENABLE_ISAC ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SILK ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_H263 ON)
set(DEFAULT_VALUE_ENABLE_H263P ON)
set(DEFAULT_VALUE_ENABLE_MPEG4 ON)
set(DEFAULT_VALUE_ENABLE_OPENH264 OFF)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_X264 ON)
set(DEFAULT_VALUE_ENABLE_TUNNEL OFF)
set(DEFAULT_VALUE_ENABLE_UNIT_TESTS ON)


# Global configuration
set(LINPHONE_BUILDER_HOST "")
if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6")
	set(CMAKE_OSX_ARCHITECTURES "i386")
	set(LINPHONE_BUILDER_HOST "i686-apple-darwin")
	set(LINPHONE_BUILDER_CPPFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
	set(LINPHONE_BUILDER_CFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
	set(LINPHONE_BUILDER_LDFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
endif(APPLE)
if(UNIX)
	# Adjust PKG_CONFIG_PATH to include install directory
	set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/:$ENV{PKG_CONFIG_PATH}:/usr/lib/pkgconfig/:/usr/lib/x86_64-linux-gnu/pkgconfig/:/usr/share/pkgconfig/:/usr/local/lib/pkgconfig/:/opt/local/lib/pkgconfig/")
endif(UNIX)


# Include builders
include(builders/CMakeLists.txt)


# xml2
list(APPEND EP_xml2_CONFIGURE_OPTIONS "--with-sax1")
