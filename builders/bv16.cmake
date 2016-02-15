############################################################################
# bv16.cmake
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

#set(EP_bv16_GIT_REPOSITORY "git://git.linphone.org/bv16.git" CACHE STRING "bv16 repository URL")
#set(EP_bv16_GIT_TAG_LATEST "linphone" CACHE STRING "bv16 tag to use when compiling latest version")
#set(EP_bv16_GIT_TAG "caeb73052b1171758acb565e7174a49fbd1427d2" CACHE STRING "bv16 tag to use")
set(EP_bv16_EXTERNAL_SOURCE_PATHS "bv16" "externals/bv16")

set(EP_bv16_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
if(MSVC)
	set(EP_bv16_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
