############################################################################
# flexisip.cmake
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

set(EP_flexisip_GIT_REPOSITORY "git://git.linphone.org/flexisip" CACHE STRING "flexisip repository URL")
set(EP_flexisip_GIT_TAG_LATEST "master" CACHE STRING "flexisip tag to use when compiling latest version")
set(EP_flexisip_GIT_TAG "cc4e47496600e9b1d3d412ce6e887275c204334b" CACHE STRING "flexisip tag to use")
set(EP_flexisip_EXTERNAL_SOURCE_PATHS "<LINPHONE_BUILDER_TOP_DIR>")
set(EP_flexisip_GROUPABLE YES)

set(EP_flexisip_CMAKE_OPTIONS )
set(EP_flexisip_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_flexisip_CONFIG_H_FILE "flexisip.spec")
set(EP_flexisip_SPEC_FILE "flexisip.spec")
lcb_dependencies("sofiasip")
if (ENABLE_CONFERENCE)
	lcb_dependencies("linphone" "belr")
endif()
if (ENABLE_PRESENCE)
	lcb_dependencies("bellesip")
endif()
if(ENABLE_SOCI_BUILD)
	lcb_dependencies("soci")
endif()
if (ENABLE_TRANSCODER)
	lcb_dependencies("ms2")
else()
	lcb_dependencies("ortp")
endif()

lcb_builder_cmake_options(flexisip
	"-DENABLE_TRANSCODER=${ENABLE_TRANSCODER}"
	"-DENABLE_ODB=${ENABLE_ODB}"
	"-DENABLE_ODBC=${ENABLE_ODBC}"
	"-DENABLE_REDIS=${ENABLE_REDIS}"
	"-DENABLE_SOCI=${ENABLE_SOCI}"
	"-DENABLE_PUSHNOTIFICATION=${ENABLE_PUSHNOTIFICATION}"
	"-DENABLE_PRESENCE=${ENABLE_PRESENCE}"
	"-DENABLE_CONFERENCE=${ENABLE_CONFERENCE}"
	"-DXSDCXX_ROOT_PATH=${XSDCXX_ROOT_PATH}"
	"-DENABLE_SNMP=${ENABLE_SNMP}"
	"-DENABLE_DOC=${ENABLE_DOC}"
	"-DENABLE_PROTOBUF=${ENABLE_PROTOBUF}"
)

