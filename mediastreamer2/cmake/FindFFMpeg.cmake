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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

set(_FFMPEG_ROOT_PATHS
	${WITH_FFMPEG}
	${CMAKE_INSTALL_PREFIX}
)

find_path(LIBAVCODEC_INCLUDE_DIRS
	NAMES libavcodec/avcodec.h
	HINTS _FFMPEG_ROOT_PATHS
	PATH_SUFFIXES include
)
if(LIBAVCODEC_INCLUDE_DIRS)
	set(HAVE_LIBAVCODEC_AVCODEC_H 1)
endif()
find_path(LIBAVUTIL_INCLUDE_DIRS
	NAMES libavutil/avutil.h
	HINTS _FFMPEG_ROOT_PATHS
	PATH_SUFFIXES include
)
if(LIBAVUTIL_INCLUDE_DIRS)
	set(HAVE_LIBAVUTIL_AVUTIL_H 1)
endif()
find_path(LIBSWSCALE_INCLUDE_DIRS
	NAMES libswscale/swscale.h
	HINTS _FFMPEG_ROOT_PATHS
	PATH_SUFFIXES include
)
if(LIBSWSCALE_INCLUDE_DIRS)
	set(HAVE_LIBSWSCALE_SWSCALE_H 1)
endif()

find_library(LIBAVCODEC_LIBRARIES
	NAMES avcodec
	HINTS _FFMPEG_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
find_library(LIBAVUTIL_LIBRARIES
	NAMES avutil
	HINTS _FFMPEG_ROOT_PATHS
	PATH_SUFFIXES bin lib
)
find_library(LIBSWSCALE_LIBRARIES
	NAMES swscale
	HINTS _FFMPEG_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

set(FFMPEG_INCLUDE_DIRS ${LIBAVCODEC_INCLUDE_DIRS} ${LIBAVUTIL_INCLUDE_DIRS} ${LIBSWSCALE_INCLUDE_DIRS})
set(FFMPEG_LIBRARIES ${LIBAVCODEC_LIBRARIES} ${LIBAVUTIL_LIBRARIES} ${LIBSWSCALE_LIBRARIES})
list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
list(REMOVE_DUPLICATES FFMPEG_LIBRARIES)

if(FFMPEG_LIBRARIES)
	cmake_push_check_state(RESET)
	list(APPEND CMAKE_REQUIRED_INCLUDES ${FFMPEG_INCLUDE_DIRS})
	list(APPEND CMAKE_REQUIRED_LIBRARIES ${FFMPEG_LIBRARIES})
	if(MSVC)
		list(APPEND CMAKE_REQUIRED_DEFINITIONS -Dinline=__inline)
	endif()
	check_symbol_exists(avcodec_get_context_defaults3 libavcodec/avcodec.h HAVE_FUN_avcodec_get_context_defaults3)
	check_symbol_exists(avcodec_open2 libavcodec/avcodec.h HAVE_FUN_avcodec_open2)
	check_symbol_exists(avcodec_encode_video2 libavcodec/avcodec.h HAVE_FUN_avcodec_encode_video2)
	check_symbol_exists(av_frame_alloc libavutil/avutil.h HAVE_FUN_av_frame_alloc)
	check_symbol_exists(av_frame_free libavutil/avutil.h HAVE_FUN_av_frame_free)
	check_symbol_exists(av_frame_unref libavutil/avutil.h HAVE_FUN_av_frame_unref)
	cmake_pop_check_state()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFMpeg
	DEFAULT_MSG
	FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES
	LIBAVCODEC_INCLUDE_DIRS LIBAVUTIL_INCLUDE_DIRS LIBSWSCALE_INCLUDE_DIRS
	LIBAVCODEC_LIBRARIES LIBAVUTIL_LIBRARIES LIBSWSCALE_LIBRARIES
)

mark_as_advanced(FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES
	LIBAVCODEC_INCLUDE_DIRS LIBAVUTIL_INCLUDE_DIRS LIBSWSCALE_INCLUDE_DIRS
	LIBAVCODEC_LIBRARIES LIBAVUTIL_LIBRARIES LIBSWSCALE_LIBRARIES)
