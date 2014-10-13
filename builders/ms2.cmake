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
if(LINPHONE_BUILDER_LATEST)
	set(EP_ms2_GIT_TAG "master")
else()
	set(EP_ms2_GIT_TAG "9118e0209870808341cf5380acc5590b13787c0d")
endif()

set(EP_ms2_CMAKE_OPTIONS )
set(EP_ms2_LINKING_TYPE "-DENABLE_STATIC=NO")
set(EP_ms2_DEPENDENCIES EP_ortp)
if(ENABLE_GSM)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_GSM=YES")
	list(APPEND EP_ms2_DEPENDENCIES EP_gsm)
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_GSM=NO")
endif()
if(ENABLE_OPUS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_OPUS=YES")
	list(APPEND EP_ms2_DEPENDENCIES EP_opus)
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_OPUS=NO")
endif()
if(ENABLE_SPEEX)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SPEEX=YES")
	list(APPEND EP_ms2_DEPENDENCIES EP_speex)
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_SPEEX=NO")
endif()
if(ENABLE_VIDEO)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VIDEO=YES")
	if(ENABLE_FFMPEG)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_FFMPEG=YES")
		list(APPEND EP_ms2_DEPENDENCIES EP_ffmpeg)
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_FFMPEG=NO")
	endif()
	if(ENABLE_VPX)
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VPX=YES")
		list(APPEND EP_ms2_DEPENDENCIES EP_vpx)
	else()
		list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VPX=NO")
	endif()
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_VIDEO=NO")
endif()
if(ENABLE_UNIT_TESTS)
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=YES")
else()
	list(APPEND EP_ms2_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=NO")
endif()
if("${BUILD_V4L}" STREQUAL "yes")
	list(APPEND EP_ms2_DEPENDENCIES EP_v4l)
endif()
if(MSVC)
	set(EP_ms2_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
