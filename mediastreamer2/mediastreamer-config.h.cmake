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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*
****************************************************************************/

#define MEDIASTREAMER_MAJOR_VERSION ${MEDIASTREAMER_MAJOR_VERSION}
#define MEDIASTREAMER_MINOR_VERSION ${MEDIASTREAMER_MINOR_VERSION}
#define MEDIASTREAMER_MICRO_VERSION ${MEDIASTREAMER_MICRO_VERSION}
#define MEDIASTREAMER_VERSION "${MEDIASTREAMER_VERSION}"

#cmakedefine HAVE_SYS_SHM_H 1
#cmakedefine HAVE_ALLOCA_H 1
#cmakedefine HAVE_DLOPEN 1

#cmakedefine WORDS_BIGENDIAN

#cmakedefine MS_FIXED_POINT
#define HAVE_NON_FREE_CODECS ${HAVE_NON_FREE_CODECS}

#cmakedefine HAVE_SRTP
#cmakedefine HAVE_ZRTP
#cmakedefine HAVE_DTLS

#cmakedefine __ALSA_ENABLED__
#cmakedefine __ARTS_ENABLED__
#cmakedefine __MACSND_ENABLED__
#cmakedefine __MAC_AQ_ENABLED__
#cmakedefine __PORTAUDIO_ENABLED__
#cmakedefine __PULSEAUDIO_ENABLED__
#cmakedefine __QSA_ENABLED__
#cmakedefine HAVE_SPEEXDSP
#cmakedefine PACKAGE_PLUGINS_DIR "${PACKAGE_PLUGINS_DIR}"
#cmakedefine PACKAGE_DATA_DIR "${PACKAGE_DATA_DIR}"
#cmakedefine PLUGINS_EXT "${PLUGINS_EXT}"

#cmakedefine HAVE_LIBAVCODEC_AVCODEC_H 1
#cmakedefine HAVE_LIBSWSCALE_SWSCALE_H 1
#cmakedefine HAVE_FUN_avcodec_get_context_defaults3 1
#cmakedefine HAVE_FUN_avcodec_open2 1
#cmakedefine HAVE_FUN_avcodec_encode_video2 1
#cmakedefine HAVE_FUN_av_frame_alloc 1
#cmakedefine HAVE_FUN_av_frame_free 1
#cmakedefine HAVE_FUN_av_frame_unref 1
#cmakedefine HAVE_GL 1
#cmakedefine HAVE_X11_XLIB_H 1
#cmakedefine HAVE_XV 1
#cmakedefine HAVE_LINUX_VIDEODEV_H 1
#cmakedefine HAVE_LINUX_VIDEODEV2_H 1
#cmakedefine HAVE_POLARSSL_SSL_H 1
#cmakedefine HAVE_PCAP 1
#cmakedefine HAVE_MATROSKA 1
#cmakedefine HAVE_VPX 1

#cmakedefine HAVE_SYS_SOUNDCARD_H 1
