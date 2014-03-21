############################################################################
# voamrwbenc.cmake
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

set(EP_voamrwbenc_URL "http://downloads.sourceforge.net/project/opencore-amr/vo-amrwbenc/vo-amrwbenc-0.1.3.tar.gz")
set(EP_voamrwbenc_USE_AUTOTOOLS "yes")
set(EP_voamrwbenc_USE_AUTOGEN "yes")
set(EP_voamrwbenc_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_TOOLCHAIN_HOST}"
)
set(EP_voamrwbenc_LINKING_TYPE "--disable-static" "--enable-shared")
set(EP_voamrwbenc_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/voamrwbenc/autogen.sh" "<SOURCE_DIR>")
set(EP_voamrwbenc_DEPENDENCIES EP_opencoreamr)
