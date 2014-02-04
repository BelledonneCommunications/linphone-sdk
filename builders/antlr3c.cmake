############################################################################
# antlr3c.cmake
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

set(AUTOGEN_COMMAND "./autogen.sh")
set(CONFIGURE_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=$ENV{HOST}"
	"--disable-static"
	"--enable-shared"
)
set(CONFIGURE_COMMAND "PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR}" "CONFIG_SITE=${AUTOTOOLS_CONFIG_SITE}" "./configure" "${CONFIGURE_OPTIONS}")
set(BUILD_COMMAND "PKG_CONFIG_LIBDIR=${PKG_CONFIG_LIBDIR}" "CONFIG_SITE=${AUTOTOOLS_CONFIG_SITE}" "make" "-C" "<SOURCE_DIR>/runtime/C/")

ExternalProject_Add(EP_antlr3c
	GIT_REPOSITORY git://git.linphone.org/antlr3.git
	GIT_TAG linphone
	CONFIGURE_COMMAND cd <SOURCE_DIR>/runtime/C/ && ${AUTOGEN_COMMAND} COMMAND cd <SOURCE_DIR>/runtime/C/ && ${CONFIGURE_COMMAND}
	BUILD_COMMAND ${BUILD_COMMAND}
	INSTALL_COMMAND make -C <SOURCE_DIR>/runtime/C/ install
	BUILD_IN_SOURCE 1
)
