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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(EP_codec2_SVN_REPOSITORY "https://svn.code.sf.net/p/freetel/code/codec2/branches/0.3/" CACHE STRING "codec2 repository URL")

set(EP_codec2_CMAKE_OPTIONS "-DBUILD_SHARED_LIBS=NO")
set(EP_codec2_EXTRA_CFLAGS "-include ${CMAKE_CURRENT_LIST_DIR}/codec2/codec2_prefixed_symbols.h")
set(EP_codec2_PATCH_COMMAND "${PATCH_PROGRAM}" "-p1" "-i" "${CMAKE_CURRENT_LIST_DIR}/codec2/no-executables.patch")
