############################################################################
# mscodec2.cmake
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(EP_mscodec2_GIT_REPOSITORY "git://git.linphone.org/mscodec2.git" CACHE STRING "mscodec2 repository URL")
set(EP_mscodec2_GIT_TAG_LATEST "master" CACHE STRING "mscodec2 tag to use when compiling latest version")
set(EP_mscodec2_GIT_TAG "1.0.2" CACHE STRING "mscodec2 tag to use")
set(EP_mscodec2_EXTERNAL_SOURCE_PATHS "mscodec2")
set(EP_mscodec2_GROUPABLE YES)

set(EP_mscodec2_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_PLUGIN_LINKING_TYPE})
set(EP_mscodec2_DEPENDENCIES EP_ms2 EP_codec2)
