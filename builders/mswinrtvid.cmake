############################################################################
# mswinrtvid.cmake
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

set(EP_mswinrtvid_GIT_REPOSITORY "git://git.linphone.org/mswinrtid.git" CACHE STRING "mswinrtvid repository URL")
set(EP_mswinrtvid_GIT_TAG_LATEST "master" CACHE STRING "mswinrtvid tag to use when compiling latest version")
set(EP_mswinrtvid_GIT_TAG "b067bca955dc170b54fffe019a6a7ce86c781c8a" CACHE STRING "mswinrtvid tag to use")
set(EP_mswinrtvid_EXTERNAL_SOURCE_PATHS "mswinrtvid")
set(EP_mswinrtvid_GROUPABLE YES)

set(EP_mswinrtvid_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_PLUGIN_LINKING_TYPE})
set(EP_mswinrtvid_DEPENDENCIES EP_ms2)
