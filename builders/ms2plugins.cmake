############################################################################
# ms2plugins.cmake
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

lcb_build_method("dummy")
lcb_dependencies("ms2")

if(ENABLE_AMRNB OR ENABLE_AMRWB)
	lcb_dependencies("msamr")
endif()
if(ENABLE_CODEC2)
	lcb_dependencies("mscodec2")
endif()
if(ENABLE_G729)
	lcb_dependencies("msbcg729")
endif()
if(ENABLE_ISAC OR ENABLE_ILBC OR ENABLE_WEBRTC_AEC)
	lcb_dependencies("mswebrtc")
endif()
if(ENABLE_SILK)
	lcb_dependencies("mssilk")
endif()
if(ENABLE_OPENH264)
	lcb_dependencies("msopenh264")
endif()
if(ENABLE_WASAPI)
	lcb_dependencies("mswasapi")
endif()
if(ENABLE_X264)
	lcb_dependencies("msx264")
endif()
if(ENABLE_VIDEO AND (CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone"))
	lcb_dependencies("mswp8vid")
endif()
if(ENABLE_VIDEO AND (CMAKE_SYSTEM_NAME STREQUAL "WindowsStore"))
	lcb_dependencies("mswinrtvid")
endif()
