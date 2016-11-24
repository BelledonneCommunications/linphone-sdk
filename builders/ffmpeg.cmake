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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(FFMPEG_FILENAME "ffmpeg-0.10.2-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${FFMPEG_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${FFMPEG_FILENAME}" STATUS FFMPEG_FILENAME_STATUS)
	list(GET FFMPEG_FILENAME_STATUS 0 FFMPEG_DOWNLOAD_STATUS)
	if(NOT FFMPEG_DOWNLOAD_STATUS)
		set(FFMPEG_PREBUILT 1)
	endif()
endif()

if(FFMPEG_PREBUILT)
	lcb_url("${CMAKE_CURRENT_BINARY_DIR}/${FFMPEG_FILENAME}")
	lcb_build_method("prebuilt")
elseif(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	# Use prebuilt library in the source tree for Windows 10
	lcb_external_source_paths("build/ffmpeg")
else()
	if(WIN32)
		set(EP_ffmpeg_PATCH_OPTIONS "--binary")
	endif()

	lcb_git_repository("git://git.linphone.org/ffmpeg.git")
	lcb_git_tag_latest("bc")
	lcb_git_tag("51aa587f7ddac63c831d73eb360e246765a2675f")
	lcb_external_source_paths("externals/ffmpeg")
	lcb_may_be_found_on_system(YES)
	lcb_ignore_warnings(YES)
	lcb_build_method("autotools")
	lcb_configure_options(
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
		lcb_configure_options(
			"--enable-decoder=h263"
			"--enable-encoder=h263"
		)
	endif()
	if(ENABLE_H263P OR IOS)
		lcb_configure_options("--enable-encoder=h263p")
	endif()
	if(ENABLE_MPEG4 OR IOS)
		lcb_configure_options(
			"--enable-decoder=mpeg4"
			"--enable-encoder=mpeg4"
		)
	endif()
	lcb_linking_type("--disable-static" "--enable-shared")
	set(FFMPEG_ARCH "${CMAKE_SYSTEM_PROCESSOR}")

	if(WIN32)
		set(FFMPEG_TARGET_OS "mingw32")
		set(FFMPEG_ARCH "i386")
		lcb_extra_cflags("-include windows.h")
		lcb_extra_ldflags("-static-libgcc")
		lcb_configure_options("--enable-runtime-cpudetect")
	else()
		if(APPLE)
			set(FFMPEG_TARGET_OS "darwin")
			if(IOS)
				lcb_configure_options(
					"--enable-decoder=h264"
					"--disable-iconv"
					"--disable-mmx"
					"--enable-cross-compile"
					"--cross-prefix=${SDK_BIN_PATH}/"
					"--sysroot=${CMAKE_OSX_SYSROOT}"
				)
				lcb_make_options("RANLIB=\"\$RANLIB\"")
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
					set(FFMPEG_ARCH "arm64")
				else()
					set(FFMPEG_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
				endif()
				if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armv7")
					lcb_configure_options("--enable-neon" "--cpu=cortex-a8" "--disable-armv5te" "--enable-armv6" "--enable-armv6t2")
				endif()
			else()
				lcb_configure_options(
					"--enable-runtime-cpudetect"
					"--sysroot=${CMAKE_OSX_SYSROOT}"
				)
			endif()
		elseif(ANDROID)
			lcb_configure_options(
				"--enable-decoder=h264"
				"--disable-iconv"
				"--disable-mmx"
				"--enable-cross-compile"
				"--cross-prefix=${TOOLCHAIN_PATH}/"
				"--sysroot=${CMAKE_SYSROOT}"
			)
			set(FFMPEG_TARGET_OS "linux")
			set(FFMPEG_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
			lcb_make_options("RANLIB=\"\$RANLIB\"")
			if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
				lcb_configure_options("--enable-neon" "--cpu=cortex-a8" "--disable-armv5te" "--enable-armv6" "--enable-armv6t2")
			else()
				lcb_configure_options("--disable-mmx" "--disable-sse2" "--disable-ssse3" "--disable-asm")
			endif()
			if(CMAKE_C_COMPILER_TARGET) # When building with clang
				lcb_configure_options("--extra-cflags=--target=${CMAKE_C_COMPILER_TARGET} --gcc-toolchain=${EXTERNAL_TOOLCHAIN_PATH}/..")
				lcb_configure_options("--extra-ldflags=--target=${CMAKE_C_COMPILER_TARGET} --gcc-toolchain=${EXTERNAL_TOOLCHAIN_PATH}/..")
			endif()
		else()
			set(FFMPEG_TARGET_OS "linux")
			lcb_configure_options("--enable-runtime-cpudetect")
		endif()
		lcb_configure_options("--cc=$CC")
	endif()

	lcb_cross_compilation_options(
		"--prefix=${CMAKE_INSTALL_PREFIX}"
		"--arch=${FFMPEG_ARCH}"
		"--target-os=${FFMPEG_TARGET_OS}"
	)

	if(ENABLE_X264)
		lcb_configure_options("--enable-decoder=h264")
	endif()
endif()
