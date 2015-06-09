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
set(EP_linphone_GIT_TAG "3.8.4")
set(EP_linphone_EXTERNAL_SOURCE_PATHS "linphone")

set(EP_linphone_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
set(EP_linphone_CMAKE_OPTIONS )
set(EP_linphone_DEPENDENCIES EP_bellesip EP_ortp EP_ms2)
if(LINPHONE_BUILDER_BUILD_DEPENDENCIES AND NOT IOS)
	# Do not build sqlite3 and xml2 on IOS, they are provided by the system
	list(APPEND EP_linphone_DEPENDENCIES EP_sqlite3 EP_xml2)
endif()
if(ENABLE_VIDEO)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_VIDEO=YES")
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_VIDEO=NO")
endif()
if(ENABLE_TUNNEL)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_TUNNEL=YES")
	list(APPEND EP_linphone_DEPENDENCIES EP_tunnel)
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_TUNNEL=NO")
endif()
if(ENABLE_UNIT_TESTS)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=YES")
	if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
		list(APPEND EP_linphone_DEPENDENCIES EP_cunit)
	endif()
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=NO")
endif()
if(ENABLE_DEBUG_LOGS)
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_DEBUG_LOGS=YES")
else()
	list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_DEBUG_LOGS=NO")
endif()
if(LINPHONE_BUILDER_BUILD_DEPENDENCIES AND NOT IOS)
	# Do not build zlib on IOS, it is provided by the system
	list(APPEND EP_linphone_DEPENDENCIES EP_zlib)
endif()
if(MSVC)
	set(EP_linphone_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
