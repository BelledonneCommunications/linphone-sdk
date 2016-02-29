############################################################################
# matroska2.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

set(EP_matroska2_GIT_REPOSITORY "git://git.linphone.org/libmatroska-c.git" CACHE STRING "matroska2 repository URL")
set(EP_matroska2_GIT_TAG_LATEST "bc" CACHE STRING "matroska2 tag to use when compiling latest version")
set(EP_matroska2_GIT_TAG "c3fc2746f18bafefe3010669d8d2855240565c86" CACHE STRING "matroska2 tag to use")
set(EP_matroska2_EXTERNAL_SOURCE_PATHS "externals/libmatroska-c")

set(EP_matroska2_LINKING_TYPE "-DENABLE_STATIC=YES" "-DENABLE_SHARED=NO")
