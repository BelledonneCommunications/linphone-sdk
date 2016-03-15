############################################################################
# bzrtp.cmake
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

set(EP_bzrtp_GIT_REPOSITORY "git://git.linphone.org/bzrtp.git" CACHE STRING "bzrtp repository URL")
set(EP_bzrtp_GIT_TAG_LATEST "master" CACHE STRING "bzrtp tag to use when compiling latest version")
set(EP_bzrtp_GIT_TAG "1.0.3" CACHE STRING "bzrtp tag to use")
set(EP_bzrtp_EXTERNAL_SOURCE_PATHS "bzrtp")
set(EP_bzrtp_GROUPABLE YES)

set(EP_bzrtp_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
set(EP_bzrtp_DEPENDENCIES EP_bctoolbox)
if(LINPHONE_BUILDER_BUILD_DEPENDENCIES AND NOT APPLE)
	# Do not build xml2 on Apple systems (Mac OS X and iOS), it is provided by the system
	list(APPEND EP_bzrtp_DEPENDENCIES EP_xml2)
endif()
if(MSVC)
	set(EP_bzrtp_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
if(MINGW)
	set(EP_bzrtp_EXTRA_CPPFLAGS "-D__USE_MINGW_ANSI_STDIO")
endif()

set(EP_bzrtp_CMAKE_OPTIONS "-DENABLE_TESTS=${ENABLE_UNIT_TESTS}")
if(ENABLE_UNIT_TESTS AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_bzrtp_DEPENDENCIES EP_cunit)
endif()

