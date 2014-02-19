############################################################################
# ms2.cmake
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

set(EP_ms2_GIT_REPOSITORY "git://git.linphone.org/mediastreamer2.git")
set(EP_ms2_GIT_TAG "77b6e16c9ef07fdbb741d220c89e749ff746d654") # Branch 'master'
set(EP_ms2_AUTOTOOLS "yes")
set(EP_ms2_CONFIGURE_OPTIONS "--disable-strict --enable-external-ortp")
set(EP_ms2_DEPENDENCIES EP_ortp)

if(${ENABLE_GSM})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --with-gsm=${CMAKE_INSTALL_PREFIX}")
	list(APPEND EP_ms2_DEPENDENCIES EP_gsm)
else(${ENABLE_GSM})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-gsm")
endif(${ENABLE_GSM})

if(${ENABLE_OPUS})
	list(APPEND EP_ms2_DEPENDENCIES EP_opus)
else(${ENABLE_OPUS})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-opus")
endif(${ENABLE_OPUS})

if(${ENABLE_SPEEX})
	list(APPEND EP_ms2_DEPENDENCIES EP_speex)
else(${ENABLE_SPEEX})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-speex")
endif(${ENABLE_SPEEX})

if(${ENABLE_FFMPEG})
	list(APPEND EP_ms2_DEPENDENCIES EP_ffmpeg)
else(${ENABLE_FFMPEG})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-ffmpeg")
endif(${ENABLE_FFMPEG})

if(${ENABLE_VPX})
	list(APPEND EP_ms2_DEPENDENCIES EP_vpx)
else(${ENABLE_VPX})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-vp8")
endif(${ENABLE_VPX})

if("${BUILD_V4L}" STREQUAL "yes")
	list(APPEND EP_ms2_DEPENDENCIES EP_v4l)
endif("${BUILD_V4L}" STREQUAL "yes")

if(${ENABLE_UNIT_TESTS})
	list(APPEND EP_ms2_DEPENDENCIES EP_cunit)
else(${ENABLE_UNIT_TESTS})
	set(EP_ms2_CONFIGURE_OPTIONS "${EP_ms2_CONFIGURE_OPTIONS} --disable-tests")
endif(${ENABLE_UNIT_TESTS})
