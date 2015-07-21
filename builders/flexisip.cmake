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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(EP_flexisip_GIT_REPOSITORY "git://git.linphone.org/flexisip")
set(EP_flexisip_GIT_TAG_LATEST "master")
set(EP_flexisip_GIT_TAG "698eb5d3f66ad9b70e07b3138e487f5f487f88bc")

set(EP_flexisip_DEPENDENCIES EP_ortp EP_sofiasip )

list(APPEND EP_flexisip_DEPENDENCIES EP_libodbmysql)

set(EP_flexisip_LINKING_TYPE "--disable-static" "--enable-shared")
set(EP_flexisip_BUILD_METHOD "autotools")
set(EP_flexisip_USE_AUTOGEN True)
set(EP_flexisip_CONFIGURE_OPTIONS )
set(EP_flexisip_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_flexisip_CONFIG_H_FILE "flexisip.spec")

if( USE_BC_ODBC )
	message(STATUS "Flexisip to be built with BC ODBC")
	list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--with-odbc=${CMAKE_INSTALL_PREFIX}")
	list(APPEND EP_flexisip_DEPENDENCIES EP_unixodbc EP_myodbc)
else()
	message(STATUS "Flexisip to be built with system ODBC")
endif()


list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--disable-transcoder" "--enable-redis" )

set(EP_flexisip_SPEC_FILE "flexisip.spec")
