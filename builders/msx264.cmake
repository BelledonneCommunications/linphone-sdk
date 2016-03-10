############################################################################
# msx264.cmake
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

set(EP_msx264_GIT_REPOSITORY "git://git.linphone.org/msx264.git" CACHE STRING "msx264 repository URL")
set(EP_msx264_GIT_TAG_LATEST "master" CACHE STRING "msx264 tag to use when compiling latest version")
set(EP_msx264_GIT_TAG "3a9b5a9ff79ea45b9f8f03d03d4a4a9213dc2c5d" CACHE STRING "msx264 tag to use")
set(EP_msx264_EXTERNAL_SOURCE_PATHS "msx264")
set(EP_msx264_GROUPABLE YES)

set(EP_msx264_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_PLUGIN_LINKING_TYPE})
set(EP_msx264_DEPENDENCIES EP_ms2 EP_x264)
