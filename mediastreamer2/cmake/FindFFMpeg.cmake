############################################################################
# FindFFMpeg.txt
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
#
# - Find the ffmpeg include file and library
#
#  FFMPEG_FOUND - system has ffmpeg
#  FFMPEG_INCLUDE_DIRS - the ffmpeg include directory
#  FFMPEG_LIBRARIES - The libraries needed to use ffmpeg

include(CMakePushCheckState)
include(CheckSymbolExists)

# The back to back user agent feature of Flexisip requires video.
# ENABLE_VIDEO strongly requires FFMpeg.

# For Arch Linux, FFMpeg 5 isn't supported with the actual code because of a definition issue in ffmpeg-private.h
# The build is working with the package ffmpeg4.4, but we have to add its path to find_path calls

find_path(LIBAVCODEC_INCLUDE_DIRS
	NAMES libavcodec/avcodec.h
	PATH_SUFFIXES include include/ffmpeg include/ffmpeg4.4
)
message(DEBUG "LIBAVCODEC_INCLUDE_DIRS: ${LIBAVCODEC_INCLUDE_DIRS}")
message(DEBUG "PACKAGENAME_ROOT: ${FFMPEG_ROOT}")
if(LIBAVCODEC_INCLUDE_DIRS)
	set(HAVE_LIBAVCODEC_AVCODEC_H 1)
endif()
find_path(LIBAVUTIL_INCLUDE_DIRS
	NAMES libavutil/avutil.h
	PATH_SUFFIXES include include/ffmpeg include/ffmpeg4.4
)
message(DEBUG "LIBAVUTIL_INCLUDE_DIRS: ${LIBAVUTIL_INCLUDE_DIRS}")
if(LIBAVUTIL_INCLUDE_DIRS)
	set(HAVE_LIBAVUTIL_AVUTIL_H 1)
endif()
find_path(LIBSWSCALE_INCLUDE_DIRS
	NAMES libswscale/swscale.h
	PATH_SUFFIXES include include/ffmpeg include/ffmpeg4.4
)
message(DEBUG "LIBSWSCALE_INCLUDE_DIRS: ${LIBSWSCALE_INCLUDE_DIRS}")
if(LIBSWSCALE_INCLUDE_DIRS)
	set(HAVE_LIBSWSCALE_SWSCALE_H 1)
endif()

if(ANDROID)
	find_library(FFMPEG_LIBRARIES
		NAMES ffmpeg-linphone
	)
else()
	find_library(LIBAVCODEC_LIBRARIES
		NAMES avcodec
		PATH_SUFFIXES bin lib bin/ffmpeg4.4 lib/ffmpeg4.4
	)
	message(DEBUG "LIBAVCODEC_LIBRARIES: ${LIBAVCODEC_LIBRARIES}")
	find_library(LIBAVUTIL_LIBRARIES
		NAMES avutil
		PATH_SUFFIXES bin lib bin/ffmpeg4.4 lib/ffmpeg4.4
	)
	message(DEBUG "LIBAVUTIL_LIBRARIES: ${LIBAVUTIL_LIBRARIES}")
	find_library(LIBSWSCALE_LIBRARIES
		NAMES swscale
		PATH_SUFFIXES bin lib bin/ffmpeg4.4 lib/ffmpeg4.4
	)
	message(DEBUG "LIBSWSCALE_LIBRARIES: ${LIBAVSWSCALE_LIBRARIES}")
endif()

find_package(Threads)
find_library(LIBM names m)

set(FFMPEG_INCLUDE_DIRS ${LIBAVCODEC_INCLUDE_DIRS} ${LIBAVUTIL_INCLUDE_DIRS} ${LIBSWSCALE_INCLUDE_DIRS})
message(DEBUG "FFMPEG_INCLUDE_DIRS: ${FFMPEG_INCLUDE_DIRS}")
if(NOT ANDROID)
	set(FFMPEG_LIBRARIES ${LIBAVCODEC_LIBRARIES} ${LIBAVUTIL_LIBRARIES} ${LIBSWSCALE_LIBRARIES})
	message(DEBUG "Not Android: FFMPEG_LIBRARIES: ${FFMPEG_LIBRARIES}")
endif()
list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
list(REMOVE_DUPLICATES FFMPEG_LIBRARIES)

if(FFMPEG_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${FFMPEG_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${FFMPEG_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
	if(MSVC)
		list(APPEND CMAKE_REQUIRED_DEFINITIONS -Dinline=__inline)
	endif()
	if(LIBM)
		list(APPEND CMAKE_REQUIRED_LIBRARIES ${LIBM})
	endif()
	check_symbol_exists(avcodec_get_context_defaults3 "libavcodec/avcodec.h" HAVE_FUN_avcodec_get_context_defaults3)
	check_symbol_exists(avcodec_open2 "libavcodec/avcodec.h" HAVE_FUN_avcodec_open2)
	check_symbol_exists(avcodec_encode_video2 "libavcodec/avcodec.h" HAVE_FUN_avcodec_encode_video2)
	check_symbol_exists(av_frame_alloc "libavutil/avutil.h;libavutil/frame.h" HAVE_FUN_av_frame_alloc)
	check_symbol_exists(av_frame_free "libavutil/avutil.h;libavutil/frame.h" HAVE_FUN_av_frame_free)
	check_symbol_exists(av_frame_unref "libavutil/avutil.h;libavutil/frame.h" HAVE_FUN_av_frame_unref)
	cmake_pop_check_state()
endif()

set(VARS FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES LIBAVCODEC_INCLUDE_DIRS LIBAVUTIL_INCLUDE_DIRS LIBSWSCALE_INCLUDE_DIRS)
message(DEBUG "FFMPEG VARS: ${VARS}")
if(NOT ANDROID)
	list(APPEND VARS LIBAVCODEC_LIBRARIES LIBAVUTIL_LIBRARIES LIBSWSCALE_LIBRARIES)
	message(DEBUG "Not Android, appending libraries to FFMPEG VARS: ${VARS}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFMpeg DEFAULT_MSG ${VARS})

mark_as_advanced(${VARS})
