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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

lcb_git_repository("git://git.linphone.org/linphone.git")
lcb_git_tag_latest("master")
lcb_git_tag("3.10.0")
lcb_external_source_paths("linphone")
lcb_groupable(YES)

lcb_dependencies("bctoolbox" "bellesip" "ortp" "ms2")
if(LINPHONE_BUILDER_BUILD_DEPENDENCIES AND NOT APPLE)
	# Do not build sqlite3, xml2 and zlib on Apple systems (Mac OS X and iOS), they are provided by the system
	lcb_dependencies("sqlite3" "xml2")
	if(NOT ANDROID AND NOT QNX)
		lcb_dependencies("zlib")
	endif()
endif()
if(ENABLE_TUNNEL)
	lcb_dependencies("tunnel")
endif()
if(ENABLE_VCARD)
	lcb_dependencies("belcard")
endif()

lcb_cmake_options(
	"-DENABLE_GTK_UI=${ENABLE_GTK_UI}"
	"-DENABLE_VIDEO=${ENABLE_VIDEO}"
	"-DENABLE_DEBUG_LOGS=${ENABLE_DEBUG_LOGS}"
	"-DENABLE_DOC=${ENABLE_DOC}"
	"-DENABLE_TOOLS=${ENABLE_TOOLS}"
	"-DENABLE_NLS=${ENABLE_NLS}"
	"-DENABLE_LIME=YES"
	"-DENABLE_UNIT_TESTS=${ENABLE_UNIT_TESTS}"
	"-DENABLE_POLARSSL=${ENABLE_POLARSSL}"
	"-DENABLE_MBEDTLS=${ENABLE_MBEDTLS}"
	"-DENABLE_TUNNEL=${ENABLE_TUNNEL}"
	"-DENABLE_VCARD=${ENABLE_VCARD}"
)

# TODO: Activate strict compilation options on IOS
if(IOS)
	lcb_cmake_options("-DENABLE_STRICT=NO")
endif()
