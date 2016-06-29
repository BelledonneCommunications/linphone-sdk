############################################################################
# bcunit.cmake
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

set(EP_bcunit_GIT_REPOSITORY "git://git.linphone.org/bcunit.git" CACHE STRING "bcunit repository URL")
set(EP_bcunit_GIT_TAG_LATEST "linphone" CACHE STRING "bcunit tag to use when compiling latest version")
set(EP_bcunit_GIT_TAG "0a0a9c60f5a1b899ae26b705fa5224ef25377982" CACHE STRING "bcunit tag to use")
set(EP_bcunit_EXTERNAL_SOURCE_PATHS "bcunit" "externals/bcunit")
set(EP_bcunit_MAY_BE_FOUND_ON_SYSTEM TRUE)
set(EP_bcunit_IGNORE_WARNINGS TRUE)

set(EP_bcunit_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
if(MSVC)
	set(EP_bcunit_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()

set(EP_bcunit_CMAKE_OPTIONS "-DENABLE_AUTOMATED=YES" "-DENABLE_CONSOLE=NO")
