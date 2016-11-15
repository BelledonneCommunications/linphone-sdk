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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

lcb_git_repository("git://git.linphone.org/bzrtp.git")
lcb_git_tag_latest("master")
lcb_git_tag("1.0.4")
lcb_external_source_paths("bzrtp")
lcb_groupable(YES)

lcb_dependencies("bctoolbox")
if(LINPHONE_BUILDER_BUILD_DEPENDENCIES)
	if(NOT APPLE)
		# Do not build xml2 on Apple systems (Mac OS X and iOS), it is provided by the system
		lcb_dependencies("xml2")
	endif()
	if(ENABLE_UNIT_TESTS)
		lcb_dependencies("bcunit")
	endif()
endif()
if(MINGW)
	lcb_extra_cppflags("-D__USE_MINGW_ANSI_STDIO")
endif()

lcb_cmake_options("-DENABLE_TESTS=${ENABLE_UNIT_TESTS}")
