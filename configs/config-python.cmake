############################################################################
# config-python.cmake
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
set(DEFAULT_VALUE_ENABLE_FFMPEG ON)
set(DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES ON)
set(DEFAULT_VALUE_ENABLE_GSM ON)
set(DEFAULT_VALUE_ENABLE_MBEDTLS ON)
set(DEFAULT_VALUE_ENABLE_MKV ON)
set(DEFAULT_VALUE_ENABLE_OPUS ON)
set(DEFAULT_VALUE_ENABLE_SPEEX ON)
set(DEFAULT_VALUE_ENABLE_SRTP ON)
set(DEFAULT_VALUE_ENABLE_VIDEO ON)
set(DEFAULT_VALUE_ENABLE_VPX ON)
set(DEFAULT_VALUE_ENABLE_WASAPI ON)
set(DEFAULT_VALUE_ENABLE_ZRTP ON)
set(DEFAULT_VALUE_ENABLE_TOOLS OFF)
set(ENABLE_NLS NO CACHE BOOL "" FORCE)

set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_STATIC=YES" "-DENABLE_SHARED=NO")


find_package(Doxygen REQUIRED)
find_package(PythonInterp REQUIRED)


# Global configuration
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)	# Restrict pkg-config to search in the install directory

if (UNIX)
	if(APPLE)
		if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
			execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/configs/python/mac_getplatform.py"
				OUTPUT_VARIABLE MAC_PLATFORM
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
			list(GET MAC_PLATFORM 0 CMAKE_OSX_DEPLOYMENT_TARGET)
		endif()
		if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.8")
			# Resolve conflict between c++ libraries when building C++11 libraries on Mac OS X 10.7
			set(LINPHONE_BUILDER_CXXFLAGS "-stdlib=libc++")
		endif()
		set(CMAKE_OSX_ARCHITECTURES "x86_64")

		set(LINPHONE_BUILDER_CPPFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
		set(LINPHONE_BUILDER_OBJCFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
		set(LINPHONE_BUILDER_LDFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
	else()
		set(LINPHONE_BUILDER_CPPFLAGS "-fPIC")
		set(LINPHONE_BUILDER_LDFLAGS "-Wl,-Bsymbolic -fPIC")
	endif()
endif()
if(WIN32)
	set(LINPHONE_BUILDER_CPPFLAGS "-D_WIN32_WINNT=0x0501 -D_ALLOW_KEYWORD_MACROS -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS")
endif()


# Include builders
include(builders/CMakeLists.txt)


# bctoolbox
lcb_builder_cmake_options(bctoolbox "-DENABLE_TESTS_COMPONENT=NO")

# ffmpeg
lcb_builder_linking_type(ffmpeg "--disable-static" "--enable-shared")

# linphone
lcb_builder_cmake_options(linphone "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(linphone "-DENABLE_CONSOLE_UI=NO")
lcb_builder_cmake_options(linphone "-DENABLE_DAEMON=NO")
lcb_builder_cmake_options(linphone "-DENABLE_NOTIFY=NO")
lcb_builder_cmake_options(linphone "-DENABLE_TUTORIALS=NO")
lcb_builder_cmake_options(linphone "-DENABLE_UNIT_TESTS=NO")
lcb_builder_cmake_options(linphone "-DENABLE_UPNP=NO")

# mbedtls
lcb_builder_linking_type(mbedtls "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

# ms2
lcb_builder_cmake_options(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
lcb_builder_cmake_options(ms2 "-DENABLE_UNIT_TESTS=NO")
lcb_builder_cmake_options(ms2 "-DENABLE_PCAP=NO")
if(UNIX AND NOT APPLE)
	lcb_builder_cmake_options(ms2 "-DENABLE_ALSA=YES")
	lcb_builder_cmake_options(ms2 "-DENABLE_PULSEAUDIO=NO")
	lcb_builder_cmake_options(ms2 "-DENABLE_OSS=NO")
	lcb_builder_cmake_options(ms2 "-DENABLE_GLX=NO")
	lcb_builder_cmake_options(ms2 "-DENABLE_X11=YES")
	lcb_builder_cmake_options(ms2 "-DENABLE_XV=YES")
endif()

# polarssl
lcb_builder_linking_type(polarssl "-DUSE_SHARED_POLARSSL_LIBRARY=NO")

# v4l
lcb_builder_linking_type(v4l "--enable-static" "--disable-shared" "--with-pic")

# vpx
lcb_builder_linking_type(vpx "--enable-static" "--disable-shared" "--enable-pic")


# Add config step to build the Python module
set(LINPHONE_BUILDER_ADDITIONAL_CONFIG_STEPS "${CMAKE_CURRENT_LIST_DIR}/python/additional_steps.cmake")
