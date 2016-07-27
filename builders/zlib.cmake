############################################################################
# zlib.cmake
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

set(EP_zlib_GIT_REPOSITORY "git://git.linphone.org/zlib.git" CACHE STRING "zlib repository URL")
set(EP_zlib_GIT_TAG_LATEST "master" CACHE STRING "zlib tag to use when compiling latest version")
set(EP_zlib_GIT_TAG "91eb77a7c5bfe7b4cc6b722aa96548d7143a9936" CACHE STRING "zlib tag to use")
set(EP_zlib_EXTERNAL_SOURCE_PATHS "externals/zlib")
set(EP_zlib_MAY_BE_FOUND_ON_SYSTEM TRUE)
set(EP_zlib_IGNORE_WARNINGS TRUE)
