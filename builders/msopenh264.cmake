############################################################################
# msopenh264.cmake
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

set(EP_msopenh264_GIT_REPOSITORY "git://git.linphone.org/msopenh264.git" CACHE STRING "msopenh264 repository URL")
set(EP_msopenh264_GIT_TAG_LATEST "master" CACHE STRING "msopenh264 tag to use when compiling latest version")
set(EP_msopenh264_GIT_TAG "1.1.1" CACHE STRING "msopenh264 tag to use")
set(EP_msopenh264_EXTERNAL_SOURCE_PATHS "msopenh264")
set(EP_msopenh264_GROUPABLE YES)

set(EP_msopenh264_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_PLUGIN_LINKING_TYPE})
set(EP_msopenh264_DEPENDENCIES EP_ms2 EP_openh264)
if(MSVC)
	set(EP_msopenh264_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
