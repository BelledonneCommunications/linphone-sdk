############################################################################
# vpx.cmake
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

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(EP_vpx_FILENAME "vpx-v1.3.0-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${EP_vpx_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${EP_vpx_FILENAME}" STATUS EP_vpx_FILENAME_STATUS)
	list(GET EP_vpx_FILENAME_STATUS 0 EP_vpx_DOWNLOAD_STATUS)
	if(NOT EP_vpx_DOWNLOAD_STATUS)
		set(EP_vpx_PREBUILT 1)
	endif()
endif()

if(EP_vpx_PREBUILT)
	set(EP_vpx_URL "${CMAKE_CURRENT_BINARY_DIR}/${EP_vpx_FILENAME}")
	set(EP_vpx_BUILD_METHOD "prebuilt")
else()
	set(EP_vpx_URL "http://storage.googleapis.com/downloads.webmproject.org/releases/webm/libvpx-1.4.0.tar.bz2")
	set(EP_vpx_URL_HASH "MD5=63b1d7f59636a42eeeee9225cc14e7de")
	set(EP_vpx_EXTERNAL_SOURCE_PATHS "externals/libvpx")
	set(EP_vpx_BUILD_METHOD "autotools")
	set(EP_vpx_DO_NOT_USE_CMAKE_FLAGS TRUE)
	set(EP_vpx_CONFIG_H_FILE vpx_config.h)
	set(EP_vpx_CONFIGURE_OPTIONS
		"--enable-error-concealment"
		"--enable-multithread"
		"--enable-realtime-only"
		"--enable-spatial-resampling"
		"--enable-vp8"
		"--disable-vp9"
		"--enable-libs"
		"--disable-install-docs"
		"--disable-debug-libs"
		"--disable-examples"
		"--disable-unit-tests"
		"--as=yasm"
	)

	if(WIN32)
		set(EP_vpx_PATCH_COMMAND "${PATCH_PROGRAM}" "-p1" "-i" "${CMAKE_CURRENT_SOURCE_DIR}/builders/vpx/enable-shared-on-windows.patch" "--binary")
		set(EP_vpx_TARGET "x86-win32-gcc")
		set(EP_vpx_LINKING_TYPE "--disable-static" "--enable-shared")
	elseif(APPLE)
		if(IOS)
			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
				set(EP_vpx_TARGET "arm64-darwin-gcc")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
				set(EP_vpx_TARGET "armv7-darwin-gcc")
			elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
				set(EP_vpx_TARGET "x86_64-iphonesimulator-gcc")
			else()
				set(EP_vpx_TARGET "x86-iphonesimulator-gcc")
			endif()
		else()
			if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
				set(EP_vpx_TARGET "x86_64-darwin10-gcc")
			else()
				set(EP_vpx_TARGET "x86-darwin10-gcc")
			endif()
			set(EP_vpx_BUILD_IN_SOURCE 1) # Build in source otherwise there are some compilation errors
		endif()
		set(EP_vpx_LINKING_TYPE "--enable-static" "--disable-shared" "--enable-pic")
	else()
		if(CMAKE_SIZEOF_VOID_P EQUAL 8)
			set(EP_vpx_TARGET "x86_64-linux-gcc")
		else()
			set(EP_vpx_TARGET "x86-linux-gcc")
		endif()
		set(EP_vpx_LINKING_TYPE "--disable-static" "--enable-shared")
	endif()

	set(EP_vpx_CROSS_COMPILATION_OPTIONS
		"--prefix=${CMAKE_INSTALL_PREFIX}"
		"--target=${EP_vpx_TARGET}"
	)
	set(EP_vpx_CONFIGURE_ENV "LD=$CC")
endif()
