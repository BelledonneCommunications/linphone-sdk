############################################################################
# mswebrtc.cmake
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

set(EP_mswebrtc_GIT_REPOSITORY "git://git.linphone.org/mswebrtc.git")
set(EP_mswebrtc_GIT_TAG_LATEST "master")
set(EP_mswebrtc_GIT_TAG "88c0fa6ae7ea35fa25eac5ce4b50898e85443ff0")
set(EP_mswebrtc_EXTERNAL_SOURCE_PATHS "mswebrtc")

set(EP_mswebrtc_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
set(EP_mswebrtc_CMAKE_OPTIONS )
set(EP_mswebrtc_DEPENDENCIES EP_ms2)

if(ENABLE_ISAC)
	list(APPEND EP_mswebrtc_CMAKE_OPTIONS "-DENABLE_ISAC=YES")
else()
	list(APPEND EP_mswebrtc_CMAKE_OPTIONS "-DENABLE_ISAC=NO")
endif()
if(ENABLE_WEBRTC_AEC)
	list(APPEND EP_mswebrtc_CMAKE_OPTIONS "-DENABLE_AECM=YES")
else()
	list(APPEND EP_mswebrtc_CMAKE_OPTIONS "-DENABLE_AECM=NO")
endif()
