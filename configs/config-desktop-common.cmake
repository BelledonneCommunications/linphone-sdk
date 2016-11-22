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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Define options that are specific to the desktop config
lcb_add_option("Theora" "Theora video encoding/decoding support." "${DEFAULT_VALUE_ENABLE_THEORA}")
lcb_add_option("Static only" "Enable compilation of libraries in static mode." "${DEFAULT_VALUE_ENABLE_STATIC_ONLY}")
lcb_add_option("Packaging" "Enable packaging" "${DEFAULT_VALUE_ENABLE_PACKAGING}")


if(ENABLE_STATIC_ONLY)
	set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_SHARED=NO" "-DENABLE_STATIC=YES")
else()
	set(DEFAULT_VALUE_CMAKE_LINKING_TYPE "-DENABLE_SHARED=YES" "-DENABLE_STATIC=NO")
endif()

# Global configuration
if(APPLE)
	if(NOT CMAKE_OSX_DEPLOYMENT_TARGET) #is it still useful?
		#without instruction chose to target lower version between current machine and current used SDK
		execute_process(COMMAND sw_vers -productVersion  COMMAND awk -F \\. "{printf \"%i.%i\",$1,$2}"  RESULT_VARIABLE sw_vers_version OUTPUT_VARIABLE CURRENT_OSX_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
		execute_process(COMMAND xcrun --sdk macosx --show-sdk-version RESULT_VARIABLE xcrun_sdk_version OUTPUT_VARIABLE CURRENT_SDK_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
		if(${CURRENT_OSX_VERSION} VERSION_LESS ${CURRENT_SDK_VERSION})
			set(CMAKE_OSX_DEPLOYMENT_TARGET ${CURRENT_OSX_VERSION})
		else()
			set(CMAKE_OSX_DEPLOYMENT_TARGET ${CURRENT_SDK_VERSION})
		endif()
	endif()
	if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS "10.8")
		# Resolve conflict between c++ libraries when building C++11 libraries on Mac OS X 10.7
		set(LINPHONE_BUILDER_CXXFLAGS "-stdlib=libc++")
	endif()

	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_OSX_ARCHITECTURES "x86_64")
		set(LINPHONE_BUILDER_HOST "x86_64-apple-darwin")
	else()
		set(CMAKE_OSX_ARCHITECTURES "i386")
		set(LINPHONE_BUILDER_HOST "i686-apple-darwin")
	endif()
	set(CMAKE_MACOSX_RPATH 1)
endif()
if(WIN32)
	set(LINPHONE_BUILDER_CPPFLAGS "-D_WIN32_WINNT=0x0600 -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS")
	if(MSVC AND MSVC_VERSION GREATER 1600) # Visual Studio 2010 defines this macro itself
		set(LINPHONE_BUILDER_CPPFLAGS "${LINPHONE_BUILDER_CPPFLAGS} -D_ALLOW_KEYWORD_MACROS")
	endif()
endif()

# Adjust PKG_CONFIG_PATH to include install directory
if(NOT LINPHONE_BUILDER_PKG_CONFIG_PATH)
	if(UNIX)
		set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/:$ENV{PKG_CONFIG_PATH}:/usr/lib/pkgconfig/:/usr/lib/x86_64-linux-gnu/pkgconfig/:/usr/share/pkgconfig/:/usr/local/lib/pkgconfig/:/opt/local/lib/pkgconfig/")
	else() # Windows
		set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/")
	endif()
endif()


include(GNUInstallDirs)
if(NOT CMAKE_INSTALL_RPATH)
	list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" _IS_SYSTEM_DIR)
	if(_IS_SYSTEM_DIR STREQUAL "-1")
		set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
		if(NOT CMAKE_INSTALL_LIBDIR STREQUAL "lib")
			list(APPEND CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
		endif()
	endif()
endif()


# Include builders
include(builders/CMakeLists.txt)

# linphone
if(WIN32)
	lcb_builder_cmake_options(linphone "-DENABLE_RELATIVE_PREFIX=YES")
else()
	lcb_builder_cmake_options(linphone "-DENABLE_RELATIVE_PREFIX=${ENABLE_RELATIVE_PREFIX}")
endif()

# ms2
if(WIN32)
	lcb_builder_cmake_options(ms2 "-DENABLE_RELATIVE_PREFIX=YES")
	lcb_builder_extra_ldflags(ms2 "/SAFESEH:NO")
else()
	lcb_builder_cmake_options(ms2 "-DENABLE_THEORA=${ENABLE_THEORA}")
	lcb_builder_cmake_options(ms2 "-DENABLE_RELATIVE_PREFIX=${ENABLE_RELATIVE_PREFIX}")
endif()

# opencoreamr
if(NOT WIN32)
	lcb_builder_extra_cflags(opencoreamr "-fPIC")
	lcb_builder_extra_cxxflags(opencoreamr "-fPIC")
	lcb_builder_extra_ldflags(opencoreamr "-fPIC")
endif()

if(ENABLE_STATIC_ONLY)
	# ffmpeg
	lcb_builder_linking_type(ffmpeg "--enable-static" "--disable-shared" "--enable-pic")

	# mbedtls
	lcb_builder_linking_type(mbedtls "-DUSE_STATIC_MBEDTLS_LIBRARY=YES" "-DUSE_SHARED_MBEDTLS_LIBRARY=NO")

	# polarssl
	lcb_builder_linking_type(polarssl "-DUSE_SHARED_POLARSSL_LIBRARY=0")
else()
	# openh264
	lcb_builder_linking_type(openh264 "-shared")
endif()

# voamrwbenc
if(NOT WIN32)
	lcb_builder_extra_cflags(voamrwbenc "-fPIC")
	lcb_builder_extra_cxxflags(voamrwbenc "-fPIC")
	lcb_builder_extra_ldflags(voamrwbenc "-fPIC")
endif()

# vpx
if(WIN32)
	lcb_linking_type(vpx "--enable-static" "--disable-shared" "--enable-pic")
endif()
