############################################################################
# belle-sip.cmake
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

set(EP_bellesip_GIT_REPOSITORY "git://git.linphone.org/belle-sip.git" CACHE STRING "bellesip repository URL")
set(EP_bellesip_GIT_TAG_LATEST "master" CACHE STRING "bellesip tag to use when compiling latest version")
set(EP_bellesip_GIT_TAG "1.4.2" CACHE STRING "bellesip tag to use")
set(EP_bellesip_EXTERNAL_SOURCE_PATHS "belle-sip")
set(EP_bellesip_GROUPABLE YES)

set(EP_bellesip_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
set(EP_bellesip_DEPENDENCIES EP_bctoolbox)

if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	if(NOT DISABLE_BC_ANTLR)
		list(APPEND EP_bellesip_DEPENDENCIES EP_antlr3c)
		message(STATUS "ENABLING ANTLR3C BUILDER")
	else()
		message(STATUS "DISABLING ANTLR3C BUILDER")
	endif()
	if (NOT QNX)
		list(APPEND EP_bellesip_DEPENDENCIES EP_zlib)
	endif()
endif()

if(MSVC)
	set(EP_bellesip_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()

set(EP_bellesip_CMAKE_OPTIONS "-DENABLE_RTP_MAP_ALWAYS_IN_SDP=${ENABLE_RTP_MAP_ALWAYS_IN_SDP}")

# TODO: Activate strict compilation options on IOS
if(IOS)
	list(APPEND EP_bellesip_CMAKE_OPTIONS "-DENABLE_STRICT=NO")
endif()

list(APPEND EP_bellesip_CMAKE_OPTIONS "-DENABLE_TUNNEL=${ENABLE_TUNNEL}")
if(ENABLE_TUNNEL)
	list(APPEND EP_bellesip_DEPENDENCIES EP_tunnel)
endif()
list(APPEND EP_bellesip_CMAKE_OPTIONS "-DENABLE_TESTS=${ENABLE_UNIT_TESTS}")

set(EP_bellesip_SPEC_FILE "belle-sip.spec")
set(EP_bellesip_RPMBUILD_NAME "belle-sip")
