/***************************************************************************
* config.h.cmake
* Copyright (C) 2014  Belledonne Communications, Grenoble France
*
****************************************************************************
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
****************************************************************************/

#define MEDIASTREAMER_MAJOR_VERSION ${MEDIASTREAMER_MAJOR_VERSION}
#define MEDIASTREAMER_MINOR_VERSION ${MEDIASTREAMER_MINOR_VERSION}
#define MEDIASTREAMER_MICRO_VERSION ${MEDIASTREAMER_MICRO_VERSION}
#define MEDIASTREAMER_VERSION "${MEDIASTREAMER_VERSION}"

#cmakedefine HAVE_INTTYPES_H
#cmakedefine HAVE_MEMORY_H
#cmakedefine HAVE_STDINT_H
#cmakedefine HAVE_STDLIB_H
#cmakedefine HAVE_STRINGS_H
#cmakedefine HAVE_STRING_H
#cmakedefine HAVE_SYS_STAT_H
#cmakedefine HAVE_SYS_TYPES_H
#cmakedefine HAVE_POLL_H
#cmakedefine HAVE_SYS_POLL_H
#cmakedefine HAVE_SYS_UIO_H
#cmakedefine HAVE_FCNTL_H
#cmakedefine HAVE_SYS_TIME_H
#cmakedefine HAVE_UNISTD_H
#cmakedefine HAVE_SYS_SHM_H
#cmakedefine HAVE_WINDOWS_H
#cmakedefine HAVE_DLFCN_H
#cmakedefine HAVE_ALLOCA_H

#cmakedefine HAVE_DLOPEN

#cmakedefine WORDS_BIGENDIAN

#cmakedefine MS_FIXED_POINT
#cmakedefine __ALSA_ENABLED__
#cmakedefine __ARTS_ENABLED__
#cmakedefine __MACSND_ENABLED__
#cmakedefine __MAC_AQ_ENABLED__
#cmakedefine __PORTAUDIO_ENABLED__
#cmakedefine __PULSEAUDIO_ENABLED__
#cmakedefine __QSA_ENABLED__
#cmakedefine ORTP_HAVE_SRTP
#cmakedefine HAVE_SPEEXDSP
#cmakedefine PACKAGE_PLUGINS_DIR "${PACKAGE_PLUGINS_DIR}"
#cmakedefine PACKAGE_DATA_DIR "${PACKAGE_DATA_DIR}"

#cmakedefine HAVE_LIBAVCODEC_AVCODEC_H
#cmakedefine HAVE_LIBSWSCALE_SWSCALE_H
#cmakedefine HAVE_FUN_avcodec_get_context_defaults3
#cmakedefine HAVE_FUN_avcodec_open2
#cmakedefine HAVE_FUN_avcodec_encode_video2
#cmakedefine HAVE_FUN_av_frame_alloc
#cmakedefine HAVE_FUN_av_frame_free
#cmakedefine HAVE_FUN_av_frame_unref
#cmakedefine HAVE_GL
#cmakedefine HAVE_X11_XLIB_H
#cmakedefine HAVE_XV
