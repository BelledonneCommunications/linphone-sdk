############################################################################
# config-ios-arm64.cmake
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

set(PLATFORM "OS")
include(configs/config-ios.cmake)
#XCode7  allows bitcode
if (NOT ${XCODE_VERSION} VERSION_LESS 7)
	set(LINPHONE_BUILDER_CPPFLAGS "${LINPHONE_BUILDER_CPPFLAGS} -fembed-bitcode")
endif()

# speex
list(APPEND EP_speex_CMAKE_OPTIONS "-DENABLE_ARM_NEON_INTRINSICS=1")
