############################################################################
# bctoolbox.cmake
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

lcb_git_repository("git://git.linphone.org/bctoolbox.git")
lcb_git_tag_latest("master")
lcb_git_tag("master")
lcb_external_source_paths("bctoolbox")
lcb_groupable(YES)
lcb_spec_file("bctoolbox.spec")

if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	if(ENABLE_MBEDTLS)
		lcb_dependencies("mbedtls")
	elseif(ENABLE_POLARSSL)
		lcb_dependencies("polarssl")
	endif()
	if(ENABLE_UNIT_TESTS)
		lcb_dependencies("bcunit")
	endif()
endif()

lcb_cmake_options(
	"-DENABLE_TESTS=${ENABLE_UNIT_TESTS}"
	"-DENABLE_TESTS_COMPONENT=${ENABLE_UNIT_TESTS}"
)

# TODO: Activate strict compilation options on IOS
if(IOS)
	lcb_cmake_options("-DENABLE_STRICT=NO")
endif()

if(EP_bctoolbox_BUILD_METHOD STREQUAL "rpm")
	set(EP_bctoolbox_CONFIGURE_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/bctoolbox/configure.sh.rpm.cmake)
	set(EP_bctoolbox_BUILD_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/bctoolbox/build.sh.rpm.cmake)
endif()
