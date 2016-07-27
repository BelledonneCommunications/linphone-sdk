############################################################################
# codec2.cmake
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
set(EP_codec2_GIT_REPOSITORY "git://git.linphone.org/codec2.git" CACHE STRING " codec2 mirror repository URL")
set(EP_codec2_GIT_TAG_LATEST "linphone" CACHE STRING "codec2 tag to use when compiling latest version")

set(EP_codec2_CMAKE_OPTIONS "-DBUILD_SHARED_LIBS=NO")
set(EP_codec2_EXTRA_CFLAGS "-include ${CMAKE_CURRENT_LIST_DIR}/codec2/codec2_prefixed_symbols.h")
set(EP_codec2_EXTERNAL_SOURCE_PATHS "externals/codec2")
