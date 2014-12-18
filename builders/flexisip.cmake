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

set(EP_flexisip_GIT_REPOSITORY "gitosis@git.linphone.org:flexisip")
set(EP_flexisip_GIT_TAG_LATEST "master")
set(EP_flexisip_GIT_TAG "7892114eb03c28a4983dd5626aed704cca9efa1b")

set(EP_flexisip_DEPENDENCIES EP_ortp EP_unixodbc EP_myodbc EP_sofiasip )

list(APPEND EP_flexisip_DEPENDENCIES EP_libodbmysql)

set(EP_flexisip_LINKING_TYPE "--disable-static" "--enable-shared")
set(EP_flexisip_BUILD_METHOD "autotools")
set(EP_flexisip_USE_AUTOGEN "yes")
set(EP_flexisip_CONFIGURE_OPTIONS )
set(EP_flexisip_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_flexisip_CONFIG_H_FILE "flexisip.spec")

list(APPEND EP_flexisip_CONFIGURE_OPTIONS "--disable-transcoder" "--enable-redis" "--with-odbc=${CMAKE_INSTALL_PREFIX}" )

set(EP_flexisip_SPEC_FILE "flexisip.spec")