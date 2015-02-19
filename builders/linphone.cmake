############################################################################
# linphone.cmake
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

set(EP_linphone_GIT_REPOSITORY "git://git.linphone.org/linphone.git")
set(EP_linphone_GIT_TAG_LATEST "master")
set(EP_linphone_GIT_TAG "e2d39cf097c2b14f4ee75f4a3df3ce49d1bbf462")

set(EP_linphone_CMAKE_OPTIONS )
set(EP_linphone_LINKING_TYPE "-DENABLE_STATIC=NO")
set(EP_linphone_DEPENDENCIES EP_bellesip EP_ortp EP_ms2 EP_sqlite3 EP_xml2)
if(ENABLE_VIDEO)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_VIDEO=YES")
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_VIDEO=NO")
endif()
if(ENABLE_TUNNEL)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_TUNNEL=YES")
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_TUNNEL=NO")
endif()
if(ENABLE_UNIT_TESTS)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=YES")
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=NO")
endif()
if(MSVC)
	set(EP_linphone_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
