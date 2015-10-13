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

set(EP_linphone_GIT_REPOSITORY "git://git.linphone.org/linphone.git" CACHE STRING "linphone repository URL")
set(EP_linphone_GIT_TAG_LATEST "master" CACHE STRING "linphone tag to use when compiling latest version")
set(EP_linphone_GIT_TAG "8b90c32dc84617899e8c5eb7f539712232d97ee2" CACHE STRING "linphone tag to use")
set(EP_linphone_EXTERNAL_SOURCE_PATHS "linphone")
set(EP_linphone_GROUPABLE YES)

set(EP_linphone_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
set(EP_linphone_DEPENDENCIES EP_bellesip EP_ortp EP_ms2)
if(LINPHONE_BUILDER_BUILD_DEPENDENCIES AND NOT IOS)
	# Do not build sqlite3, xml2 and zlib on IOS, they are provided by the system
	list(APPEND EP_linphone_DEPENDENCIES EP_sqlite3 EP_xml2)
	if (NOT QNX)
		list(APPEND EP_linphone_DEPENDENCIES EP_zlib)
	endif()
endif()
if(MSVC)
	set(EP_linphone_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()

set(EP_linphone_CMAKE_OPTIONS
	"-DENABLE_VIDEO=${ENABLE_VIDEO}"
	"-DENABLE_DEBUG_LOGS=${ENABLE_DEBUG_LOGS}"
	"-DENABLE_DOC=${ENABLE_DOC}"
	"-DENABLE_NLS=${ENABLE_NLS}"
)
list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_TUNNEL=${ENABLE_TUNNEL}")
if(ENABLE_TUNNEL)
	list(APPEND EP_linphone_DEPENDENCIES EP_tunnel)
endif()
list(APPEND EP_linphone_CMAKE_OPTIONS "-DENABLE_UNIT_TESTS=${ENABLE_UNIT_TESTS}")
if(ENABLE_UNIT_TESTS AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_linphone_DEPENDENCIES EP_cunit)
endif()
