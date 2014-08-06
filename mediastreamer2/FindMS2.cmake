############################################################################
# FindMS2.txt
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
# - Find the mediastreamer2 include file and library
#
#  MS2_FOUND - system has mediastreamer2
#  MS2_INCLUDE_DIRS - the mediastreamer2 include directory
#  MS2_LIBRARIES - The libraries needed to use mediastreamer2
#  MS2_CPPFLAGS - The compilation flags needed to use mediastreamer2

find_package(ORTP REQUIRED)
find_package(GSM)
find_package(Opus)
find_package(Speex)
find_package(ALSA)
find_package(Arts)
find_package(PortAudio)
find_package(PulseAudio)
find_package(QSA)
find_package(FFMpeg)
find_package(X11)
if(X11_FOUND)
	find_package(Xv)
endif()
find_package(GLX)
find_package(VPX)

set(_MS2ROOT_PATHS
	${WITH_MS2}
	${CMAKE_INSTALL_PREFIX}
)

find_path(MS2_INCLUDE_DIRS
	NAMES mediastreamer2/mscommon.h
	HINTS _MS2_ROOT_PATHS
	PATH_SUFFIXES include
)

if(MS2_INCLUDE_DIRS)
	set(HAVE_MEDIASTREAMER2_MSCOMMON_H 1)
endif()

find_library(MS2_BASE_LIBRARY
	NAMES mediastreamer_base
	HINTS ${_MS2_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)
find_library(MS2_VOIP_LIBRARY
	NAMES mediastreamer_voip
	HINTS ${_MS2_ROOT_PATHS}
	PATH_SUFFIXES bin lib
)
set(MS2_LIBRARIES ${MS2_BASE_LIBRARY} ${MS2_VOIP_LIBRARY} ${ORTP_LIBRARIES})
list(APPEND MS2_INCLUDE_DIRS ${ORTP_INCLUDE_DIRS})

if(GSM_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${GSM_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${GSM_LIBRARIES})
endif()
if(OPUS_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${OPUS_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${OPUS_LIBRARIES})
endif()
if(SPEEX_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${SPEEX_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${SPEEX_LIBRARIES})
endif()
if(ALSA_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${ALSA_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${ALSA_LIBRARIES})
endif()
if(ARTS_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${ARTS_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${ARTS_LIBRARIES})
endif()
if(PORTAUDIO_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${PORTAUDIO_LIBRARIES})
endif()
if(PULSEAUDIO_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${PULSEAUDIO_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${PULSEAUDIO_LIBRARIES})
endif()
if(QSA_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${QSA_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${QSA_LIBRARIES})
endif()
if(FFMPEG_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${FFMPEG_LIBRARIES})
endif()
if(X11_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${X11_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${X11_LIBRARIES})
endif()
if(XV_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${XV_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${XV_LIBRARIES})
endif()
if(GLX_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${GLX_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${GLX_LIBRARIES})
endif()
if(VPX_FOUND)
	list(APPEND MS2_INCLUDE_DIRS ${VPX_INCLUDE_DIRS})
	list(APPEND MS2_LIBRARIES ${VPX_LIBRARIES})
endif()
if(WIN32)
	list(APPEND MS2_LIBRARIES ole32 oleaut32 uuid gdi32 user32 vfw32)
endif(WIN32)
list(REMOVE_DUPLICATES MS2_INCLUDE_DIRS)
list(REMOVE_DUPLICATES MS2_LIBRARIES)
set(MS2_CPPFLAGS ${ORTP_CPPFLAGS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MS2
	DEFAULT_MSG
	MS2_INCLUDE_DIRS MS2_LIBRARIES MS2_BASE_LIBRARY MS2_VOIP_LIBRARY MS2_CPPFLAGS
)

mark_as_advanced(MS2_INCLUDE_DIRS MS2_LIBRARIES MS2_BASE_LIBRARY MS2_VOIP_LIBRARY MS2_CPPFLAGS)
