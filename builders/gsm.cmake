############################################################################
# gsm.cmake
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

set(EP_gsm_GIT_REPOSITORY "git://git.linphone.org/gsm.git")
set(EP_gsm_GIT_TAG_LATEST "linphone")
set(EP_gsm_GIT_TAG "8722e2c643ab6cc727c62fc960cbefd43e024329")

set(EP_gsm_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/gsm/CMakeLists.txt" "<SOURCE_DIR>")
set(EP_gsm_LINKING_TYPE "-DENABLE_STATIC=0")
if(MSVC)
	set(EP_gsm_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
