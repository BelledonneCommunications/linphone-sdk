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

if(EP_flexisip_FORCE_AUTOTOOLS)
	set(EP_flexisip_LINKING_TYPE "--disable-static" "--enable-shared")
	set(EP_flexisip_BUILD_METHOD "autotools")
else()
	set(EP_flexisip_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
endif()
set(EP_flexisip_USE_AUTOGEN True)
set(EP_flexisip_CONFIGURE_OPTIONS )
set(EP_flexisip_CMAKE_OPTIONS )
set(EP_flexisip_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_flexisip_CONFIG_H_FILE "flexisip.spec")
set(EP_flexisip_SPEC_FILE "flexisip.spec")
set(EP_flexisip_DEPENDENCIES EP_sofiasip )
if (ENABLE_PRESENCE)
	list(APPEND EP_flexisip_DEPENDENCIES EP_bellesip)
endif()
if(ENABLE_SOCI_BUILD)
	list(APPEND EP_flexisip_DEPENDENCIES EP_soci)
endif()
if (ENABLE_TRANSCODER)
	list(APPEND EP_flexisip_DEPENDENCIES EP_ms2)
else()
	list(APPEND EP_flexisip_DEPENDENCIES EP_ortp)
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--disable-transcoder")
endif()

list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_TRANSCODER=${ENABLE_TRANSCODER}")

list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_ODB=${ENABLE_ODB}")
if(ENABLE_ODB)
	message(STATUS "Flexisip to be built with ODB")
	list(APPEND EP_flexisip_DEPENDENCIES EP_libodbmysql)
else()
	message(STATUS "Flexisip to be built without ODB")
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--enable-libodb=no")
endif()
list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_ODBC=${ENABLE_ODBC}")

if(ENABLE_ODBC)
	if(ENABLE_BC_ODBC)
		message(STATUS "Flexisip to be built with BC ODBC")
		list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--with-odbc=${CMAKE_INSTALL_PREFIX}")
		list(APPEND EP_flexisip_DEPENDENCIES EP_unixodbc EP_myodbc)
	else()
		message(STATUS "Flexisip to be built with system ODBC")
	endif()
else()
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--disable-odbc")
endif()

list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_REDIS=${ENABLE_REDIS}")
if(ENABLE_REDIS)
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--enable-redis")
else()
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--disable-redis")
endif()

list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_SOCI=${ENABLE_SOCI}")
if(ENABLE_SOCI)
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--enable-soci")
else()
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--disable-soci")
endif()
if(ENABLE_SOCI_BUILD)
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--with-soci=/usr")
endif()

list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_PUSHNOTIFICATION=${ENABLE_PUSHNOTIFICATION}")
if(ENABLE_PUSHNOTIFICATION)
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--enable-pushnotification")
else()
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--disable-pushnotification")
endif()

list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_PRESENCE=${ENABLE_PRESENCE}")
list(APPEND EP_flexisip_CMAKE_OPTIONS "-DXSDCXX_ROOT_PATH=${XSDCXX_ROOT_PATH}")
list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_SNMP=${ENABLE_SNMP}")
list(APPEND EP_flexisip_CMAKE_OPTIONS "-DENABLE_DOC=${ENABLE_DOC}")
