############################################################################
# ffmpeg.cmake
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
	set(EP_ffmpeg_FILENAME "ffmpeg-0.10.2-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${EP_ffmpeg_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${EP_ffmpeg_FILENAME}" STATUS EP_ffmpeg_FILENAME_STATUS)
	list(GET EP_ffmpeg_FILENAME_STATUS 0 EP_ffmpeg_DOWNLOAD_STATUS)
	if(NOT EP_ffmpeg_DOWNLOAD_STATUS)
		set(EP_ffmpeg_PREBUILT 1)
	endif()
endif()

if(EP_ffmpeg_PREBUILT)
	set(EP_ffmpeg_URL "${CMAKE_CURRENT_BINARY_DIR}/${EP_ffmpeg_FILENAME}")
	set(EP_ffmpeg_BUILD_METHOD "prebuilt")
elseif(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	# Use prebuilt library in the source tree for Windows 10
	set(EP_ffmpeg_EXTERNAL_SOURCE_PATHS "build/ffmpeg")
else()
	if(WIN32)
		set(EP_ffmpeg_PATCH_OPTIONS "--binary")
	endif()

	set(EP_ffmpeg_GIT_REPOSITORY "git://git.linphone.org/ffmpeg.git" CACHE STRING "ffmpeg repository URL")
	set(EP_ffmpeg_GIT_TAG_LATEST "bc" CACHE STRING "ffmpeg tag to use when compiling latest version")
	set(EP_ffmpeg_GIT_TAG "51aa587f7ddac63c831d73eb360e246765a2675f" CACHE STRING "ffmpeg tag to use")
	set(EP_ffmpeg_EXTERNAL_SOURCE_PATHS "externals/ffmpeg")
	set(EP_ffmpeg_MAY_BE_FOUND_ON_SYSTEM TRUE)
	set(EP_ffmpeg_IGNORE_WARNINGS TRUE)
	set(EP_ffmpeg_BUILD_METHOD "autotools")
	set(EP_ffmpeg_CONFIGURE_OPTIONS
		"--disable-doc"
		"--disable-zlib"
		"--disable-bzlib"
		"--disable-ffplay"
		"--disable-ffprobe"
		"--disable-ffserver"
		"--disable-avdevice"
		"--disable-avfilter"
		"--disable-network"
		"--disable-avformat"
		"--disable-everything"
		"--enable-decoder=mjpeg"
		"--enable-encoder=mjpeg"
		# Disable video acceleration support for compatibility with older Mac OS X versions (vda, vaapi, vdpau).
		"--disable-vda"
		"--disable-vaapi"
		"--disable-vdpau"
		"--ar=\$AR"
		"--cc=\$CC"
		"--nm=\$NM"
		"--extra-cflags=\$CFLAGS -w"
		"--extra-cxxflags=\$CXXFLAGS"
		"--extra-ldflags=\$LDFLAGS"
	)
	if(ENABLE_H263 OR IOS)
		list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS
			"--enable-decoder=h263"
			"--enable-encoder=h263"
		)
	endif()
	if(ENABLE_H263P OR IOS)
		list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--enable-encoder=h263p")
	endif()
	if(ENABLE_MPEG4 OR IOS)
		list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS
			"--enable-decoder=mpeg4"
			"--enable-encoder=mpeg4"
		)
	endif()
	set(EP_ffmpeg_LINKING_TYPE "--disable-static" "--enable-shared")
	set(EP_ffmpeg_ARCH "${CMAKE_SYSTEM_PROCESSOR}")

	if(WIN32)
		set(EP_ffmpeg_TARGET_OS "mingw32")
		set(EP_ffmpeg_ARCH "i386")
		set(EP_ffmpeg_EXTRA_CFLAGS "-include windows.h")
		set(EP_ffmpeg_EXTRA_LDFLAGS "-static-libgcc")
		list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--enable-runtime-cpudetect")
	else()
		if(APPLE)
			set(EP_ffmpeg_TARGET_OS "darwin")
			if(IOS)
				list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS
					"--enable-decoder=h264"
					"--disable-iconv"
					"--disable-mmx"
					"--enable-cross-compile"
					"--cross-prefix=${SDK_BIN_PATH}/"
					"--sysroot=${CMAKE_OSX_SYSROOT}"
				)
				set(EP_ffmpeg_MAKE_OPTIONS "RANLIB=\"\$RANLIB\"")
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
					set(EP_ffmpeg_ARCH "arm64")
				else()
					set(EP_ffmpeg_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
				endif()
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
					list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--enable-neon" "--cpu=cortex-a8" "--disable-armv5te" "--enable-armv6" "--enable-armv6t2")
				endif()
			else()
				list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS
					"--enable-runtime-cpudetect"
					"--sysroot=${CMAKE_OSX_SYSROOT}"
				)
			endif()
		elseif(ANDROID)
			list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS
				"--enable-decoder=h264"
				"--disable-iconv"
				"--disable-mmx"
				"--enable-cross-compile"
				"--cross-prefix=${ANDROID_TOOLCHAIN_PATH}/"
				"--sysroot=${CMAKE_SYSROOT}"
			)
			set(EP_ffmpeg_TARGET_OS "linux")
			set(EP_ffmpeg_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
			set(EP_ffmpeg_MAKE_OPTIONS "RANLIB=\"\$RANLIB\"")
			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
				list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--enable-neon" "--cpu=cortex-a8" "--disable-armv5te" "--enable-armv6" "--enable-armv6t2")
			else()
				list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--disable-mmx" "--disable-sse2" "--disable-ssse3")
			endif()
			if(CMAKE_C_COMPILER_TARGET) # When building with clang
				list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--extra-cflags=--target=${CMAKE_C_COMPILER_TARGET} --gcc-toolchain=${EXTERNAL_TOOLCHAIN_PATH}/..")
				list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--extra-ldflags=--target=${CMAKE_C_COMPILER_TARGET} --gcc-toolchain=${EXTERNAL_TOOLCHAIN_PATH}/..")
			endif()
		else()
			set(EP_ffmpeg_TARGET_OS "linux")
			list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--enable-runtime-cpudetect")
		endif()
		list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--cc=$CC")
	endif()

	set(EP_ffmpeg_CROSS_COMPILATION_OPTIONS
		"--prefix=${CMAKE_INSTALL_PREFIX}"
		"--arch=${EP_ffmpeg_ARCH}"
		"--target-os=${EP_ffmpeg_TARGET_OS}"
	)

	if(ENABLE_X264)
		list(APPEND EP_ffmpeg_CONFIGURE_OPTIONS "--enable-decoder=h264")
	endif()
endif()
