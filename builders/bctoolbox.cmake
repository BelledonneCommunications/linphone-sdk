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

set(EP_bctoolbox_GIT_REPOSITORY "git://git.linphone.org/bctoolbox.git" CACHE STRING "bctoolbox repository URL")
set(EP_bctoolbox_GIT_TAG_LATEST "master" CACHE STRING "bctoolbox tag to use when compiling latest version")
set(EP_bctoolbox_GIT_TAG "master" CACHE STRING "bctoolbox tag to use")
set(EP_bctoolbox_EXTERNAL_SOURCE_PATHS "bctoolbox")
set(EP_bctoolbox_GROUPABLE YES)

set(EP_bctoolbox_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
set(EP_bctoolbox_DEPENDENCIES )
if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	if(ENABLE_MBEDTLS)
		list(APPEND EP_bctoolbox_DEPENDENCIES EP_mbedtls)
	elseif(ENABLE_POLARSSL)
		list(APPEND EP_bctoolbox_DEPENDENCIES EP_polarssl)
	endif()
endif()
list(APPEND EP_bctoolbox_CMAKE_OPTIONS "-DENABLE_TESTS=${ENABLE_UNIT_TESTS}")
if(ENABLE_UNIT_TESTS AND LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	list(APPEND EP_bctoolbox_DEPENDENCIES EP_cunit)
endif()

if(MSVC)
	set(EP_bctoolbox_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
# TODO: Activate strict compilation options on IOS
if(IOS)
	list(APPEND EP_bctoolbox_CMAKE_OPTIONS "-DENABLE_STRICT=NO")
endif()

if(EP_bctoolbox_BUILD_METHOD STREQUAL "rpm")
	set(EP_bctoolbox_SPEC_FILE "bctoolbox.spec")
	set(EP_bctoolbox_CONFIGURE_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/bctoolbox/configure.sh.rpm.cmake)
	set(EP_bctoolbox_BUILD_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/bctoolbox/build.sh.rpm.cmake)
endif()

#
#set(EP_bctoolbox_RPMBUILD_NAME "belle-sip")
