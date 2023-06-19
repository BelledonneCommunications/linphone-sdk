############################################################################
# FindFFMpeg.cmake
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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
# Find the ffmpeg libraries: avcodec, avutil and swscale.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  avcodec - If the avcodec library has been found
#  avutil - If the avutil library has been found
#  swscale - If the swscale library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  FFMpeg_FOUND - The ffmpeg libraries have been found
#  FFMpeg_TARGETS - The list of the names of the CMake targets for the ffmpeg libraries
#  FFMpeg_TARGET - The name of the CMake target for the ffmpeg library (only for Android)
#  FFMpeg_avcodec_TARGET - The name of the CMake target for the avcodec library (when not on Android)
#  FFMPEG_avutil_TARGET - The name of the CMake target for the avutil library (when not on Android)
#  FFMPEG_swscale_TARGET - The name of the CMake target for the swscale library (when not on Android)
#  HAVE_LIBAVCODEC_AVCODEC_H - If the libavcodec/avcodec.h header file is available
#  HAVE_LIBSWSCALE_SWSCALE_H - If the libswscale/swscale.h header file is available
#  HAVE_FUN_avcodec_get_context_defaults3 - If the avcodec library defines the avcodec_get_context_defaults3 symbol
#  HAVE_FUN_avcodec_open2 - If the avcodec library defines the avcodec_open2 symbol
#  HAVE_FUN_avcodec_encode_video2 - If the avcodec library defines the avcodec_encode_video2 symbol
#  HAVE_FUN_av_frame_alloc - If the avutil library defines the av_frame_alloc symbol
#  HAVE_FUN_av_frame_free - If the avutil library defines the av_frame_free symbol
#  HAVE_FUN_av_frame_unref - If the avutil library defines the av_frame_unref symbol


# The back to back user agent feature of Flexisip requires video.
# ENABLE_VIDEO strongly requires FFMpeg.

# For Arch Linux, FFMpeg 5 isn't supported with the actual code because of a definition issue in ffmpeg-private.h
# The build is working with the package ffmpeg4.4, but we have to add its path to find_path calls


include(FindPackageHandleStandardArgs)

set(_FFMpeg_REQUIRED_VARS FFMpeg_TARGETS)
if(ANDROID)
	list(APPEND _FFMpeg_REQUIRED_VARS FFMpeg_TARGET)
else()
	list(APPEND _FFMpeg_REQUIRED_VARS FFMpeg_avcodec_TARGET FFMpeg_avutil_TARGET FFMpeg_swscale_TARGET)
endif()
set(_FFMpeg_CACHE_VARS ${_FFMpeg_REQUIRED_VARS}
	HAVE_LIBAVCODEC_AVCODEC_H
	HAVE_LIBSWSCALE_SWSCALE_H
	HAVE_FUN_avcodec_get_context_defaults3
	HAVE_FUN_avcodec_open2
	HAVE_FUN_avcodec_encode_video2
	HAVE_FUN_av_frame_alloc
	HAVE_FUN_av_frame_free
	HAVE_FUN_av_frame_unref
)

if(TARGET ffmpeg)

	set(FFMpeg_avcodec_TARGET avcodec)
	set(FFMpeg_avutil_TARGET avutil)
	set(FFMpeg_swscale_TARGET swscale)
	set(FFMpeg_TARGETS ${FFMpeg_avcodec_TARGET} ${FFMpeg_avutil_TARGET} ${FFMpeg_swscale_TARGET})
	set(HAVE_LIBAVCODEC_AVCODEC_H 1)
	set(HAVE_LIBSWSCALE_SWSCALE_H 1)
	set(HAVE_FUN_avcodec_get_context_defaults3 1)
	set(HAVE_FUN_avcodec_open2 1)

else()

	find_path(_FFMpeg_avcodec_INCLUDE_DIRS
		NAMES libavcodec/avcodec.h
		PATH_SUFFIXES include include/ffmpeg include/ffmpeg4.4
	)
	find_path(_FFMpeg_avutil_INCLUDE_DIRS
		NAMES libavutil/avutil.h
		PATH_SUFFIXES include include/ffmpeg include/ffmpeg4.4
	)
	find_path(_FFMpeg_swscale_INCLUDE_DIRS
		NAMES libswscale/swscale.h
		PATH_SUFFIXES include include/ffmpeg include/ffmpeg4.4
	)

	if(_FFMpeg_avcodec_INCLUDE_DIRS AND _FFMpeg_avutil_INCLUDE_DIRS AND _FFMpeg_swscale_INCLUDE_DIRS)
		set(HAVE_LIBAVCODEC_AVCODEC_H 1)
		set(HAVE_LIBSWSCALE_SWSCALE_H 1)

		if(ANDROID)
			find_library(_FFMpeg_LIBRARY NAMES ffmpeg-linphone)
			set(_FFMpeg_LIBRARIES ${_FFMpeg_LIBRARY})
		else()
			find_library(_FFMpeg_avcodec_LIBRARY
				NAMES avcodec
				PATH_SUFFIXES bin lib bin/ffmpeg4.4 lib/ffmpeg4.4
			)
			find_library(_FFMpeg_avutil_LIBRARY
				NAMES avutil
				PATH_SUFFIXES bin lib bin/ffmpeg4.4 lib/ffmpeg4.4
			)
			find_library(_FFMpeg_swscale_LIBRARY
				NAMES swscale
				PATH_SUFFIXES bin lib bin/ffmpeg4.4 lib/ffmpeg4.4
			)
			set(_FFMpeg_LIBRARIES ${_FFMpeg_avcodec_LIBRARY} ${_FFMpeg_avutil_LIBRARY} ${_FFMpeg_swscale_LIBRARY})
		endif()

		if(_FFMpeg_LIBRARIES)
			include(CMakePushCheckState)
			include(CheckSymbolExists)

			find_package(Threads)
			find_library(LIBM names m)

			set(_FFMpeg_INCLUDE_DIRS ${_FFMpeg_avcodec_INCLUDE_DIRS} ${_FFMpeg_avutil_INCLUDE_DIRS} ${_FFMpeg_swscale_INCLUDE_DIRS})
			cmake_push_check_state(RESET)
			list(APPEND CMAKE_REQUIRED_INCLUDES ${_FFMpeg_INCLUDE_DIRS})
			list(APPEND CMAKE_REQUIRED_LIBRARIES ${_FFMpeg_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
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

		if(ANDROID AND _FFMpeg_LIBRARY)
			add_library(ffmpeg UNKNOWN IMPORTED)
			set_target_properties(ffmpeg PROPERTIES
				INTERFACE_INCLUDE_DIRECTORIES "${_FFMpeg_avcodec_INCLUDE_DIRS}" "${_FFMpeg_avutil_INCLUDE_DIRS}" "${_FFMpeg_swscale_INCLUDE_DIRS}"
				IMPORTED_LOCATION "${_FFMpeg_LIBRARY}"
			)
			set(FFMpeg_TARGETS ffmpeg)
		else()
			set(FFMpeg_TARGETS )
			if(_FFMpeg_avcodec_LIBRARY)
				add_library(avcodec UNKNOWN IMPORTED)
				if(WIN32)
					set_target_properties(avcodec PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES "${_FFMpeg_avcodec_INCLUDE_DIRS}"
						IMPORTED_IMPLIB "${_FFMpeg_avcodec_LIBRARY}"
					)
				else()
					set_target_properties(avcodec PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES "${_FFMpeg_avcodec_INCLUDE_DIRS}"
						IMPORTED_LOCATION "${_FFMpeg_avcodec_LIBRARY}"
					)
				endif()
				set(FFMpeg_avcodec_TARGET avcodec)
				list(APPEND FFMpeg_TARGETS avcodec)
			endif()
			if(_FFMpeg_avutil_LIBRARY)
				add_library(avutil UNKNOWN IMPORTED)
				if(WIN32)
					set_target_properties(avutil PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES "${_FFMpeg_avutil_INCLUDE_DIRS}"
						IMPORTED_IMPLIB "${_FFMpeg_avutil_LIBRARY}"
					)
				else()
					set_target_properties(avutil PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES "${_FFMpeg_avutil_INCLUDE_DIRS}"
						IMPORTED_LOCATION "${_FFMpeg_avutil_LIBRARY}"
					)
				endif()
				set(FFMpeg_avutil_TARGET avutil)
				list(APPEND FFMpeg_TARGETS avutil)
			endif()
			if(_FFMpeg_swscale_LIBRARY)
				add_library(swscale UNKNOWN IMPORTED)
				if(WIN32)
					set_target_properties(swscale PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES "${_FFMpeg_swscale_INCLUDE_DIRS}"
						IMPORTED_IMPLIB "${_FFMpeg_swscale_LIBRARY}"
					)
				else()
					set_target_properties(swscale PROPERTIES
						INTERFACE_INCLUDE_DIRECTORIES "${_FFMpeg_swscale_INCLUDE_DIRS}"
						IMPORTED_LOCATION "${_FFMpeg_swscale_LIBRARY}"
					)
				endif()
				set(FFMpeg_swscale_TARGET swscale)
				list(APPEND FFMpeg_TARGETS swscale)
			endif()
		endif()
	endif()

endif()

find_package_handle_standard_args(FFMpeg
	REQUIRED_VARS ${_FFMpeg_REQUIRED_VARS}
)
mark_as_advanced(${_FFMpeg_CACHE_VARS})
