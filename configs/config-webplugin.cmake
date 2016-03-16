############################################################################
# config-webplugin.cmake
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
set(DEFAULT_VALUE_ENABLE_FFMPEG ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_POLARSSL ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI ON)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=NO")


# Global configuration
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)	# Restrict pkg-config to search in the install directory

if (UNIX)
	if(APPLE)
		if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
			set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6")
		endif()
		if(NOT CMAKE_OSX_ARCHITECTURES)
			set(CMAKE_OSX_ARCHITECTURES "i386")
		endif()
		set(LINPHONE_BUILDER_HOST "${CMAKE_OSX_ARCHITECTURES}-apple-darwin")
		set(LINPHONE_BUILDER_CPPFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
		set(LINPHONE_BUILDER_OBJCFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
		set(LINPHONE_BUILDER_LDFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
	else()
		set(LINPHONE_BUILDER_LDFLAGS "-Wl,-Bsymbolic")
	endif()
endif()
if(WIN32)
	set(LINPHONE_BUILDER_CPPFLAGS "-D_WIN32_WINNT=0x0501 -D_ALLOW_KEYWORD_MACROS")
endif()


# Include builders
include(builders/CMakeLists.txt)


# belle-sip
list(APPEND EP_bellesip_CMAKE_OPTIONS "-DENABLE_SERVER_SOCKETS=0")

# gsm
set(EP_gsm_LINKING_TYPE "-DENABLE_STATIC=YES")

# linphone
list(APPEND EP_linphone_CMAKE_OPTIONS
	"-DENABLE_RELATIVE_PREFIX=YES"
	"-DENABLE_CONSOLE_UI=NO"
	"-DENABLE_GTK_UI=NO"
	"-DENABLE_NOTIFY=NO"
	"-DENABLE_TOOLS=YES"
	"-DENABLE_TUTORIALS=NO"
	"-DENABLE_UNIT_TESTS=NO"
	"-DENABLE_UPNP=NO"
	"-DENABLE_NLS=NO"
)
if(MSVC)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_TOOLS=NO")
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_TOOLS=YES")
endif()

# mediastreamer2
list(APPEND EP_ms2_CMAKE_OPTIONS
	"-DENABLE_RELATIVE_PREFIX=YES"
	"-DENABLE_UNIT_TESTS=NO"
	"-DENABLE_TOOLS=NO"
)
if(UNIX AND NOT APPLE)
	list(APPEND EP_ms2_CMAKE_OPTIONS
		"-DENABLE_ALSA=YES"
		"-DENABLE_PULSEAUDIO=NO"
		"-DENABLE_OSS=NO"
		"-DENABLE_GLX=NO"
		"-DENABLE_X11=YES"
		"-DENABLE_XV=YES"
	)
endif()

# opencoreamr
if(NOT WIN32)
	set(EP_opencoreamr_EXTRA_CFLAGS "${EP_opencoreamr_EXTRA_CFLAGS} -fPIC")
	set(EP_opencoreamr_EXTRA_CXXFLAGS "${EP_opencoreamr_EXTRA_CXXFLAGS} -fPIC")
	set(EP_opencoreamr_EXTRA_LDFLAGS "${EP_opencoreamr_EXTRA_LDFLAGS} -fPIC")
endif()

# opus
set(EP_opus_LINKING_TYPE "-DENABLE_STATIC=YES")

# sqlite3
set(EP_sqlite3_LINKING_TYPE "-DENABLE_STATIC=YES")

# v4l
set(EP_v4l_LINKING_TYPE "--enable-static" "--disable-shared" "--with-pic")

# voamrwbenc
if(NOT WIN32)
	set(EP_voamrwbenc_EXTRA_CFLAGS "${EP_voamrwbenc_EXTRA_CFLAGS} -fPIC")
	set(EP_voamrwbenc_EXTRA_CXXFLAGS "${EP_voamrwbenc_EXTRA_CXXFLAGS} -fPIC")
	set(EP_voamrwbenc_EXTRA_LDFLAGS "${EP_voamrwbenc_EXTRA_LDFLAGS} -fPIC")
endif()

# vpx
set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")

# xml2
set(EP_xml2_LINKING_TYPE "-DENABLE_STATIC=YES")
