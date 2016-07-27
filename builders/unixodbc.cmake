############################################################################
# unixodbc.cmake
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

set(EP_unixodbc_GIT_REPOSITORY "git://git.linphone.org/unixODBC.git" CACHE STRING "unixodbc repository URL")
set(EP_unixodbc_GIT_TAG_LATEST "master" CACHE STRING "unixodbc tag to use when compiling latest version")
set(EP_unixodbc_GIT_TAG "9558dcfdfa89c75699d4d47c0cf4ae14962a3374" CACHE STRING "unixodbc tag to use")

set(EP_unixodbc_DEPENDENCIES )
set(EP_unixodbc_LINKING_TYPE "--disable-static" "--enable-shared")
set(EP_unixodbc_BUILD_METHOD "autotools")
set(EP_unixodbc_USE_AUTOGEN True)
set(EP_unixodbc_CONFIGURE_OPTIONS )
set(EP_unixodbc_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)

list(APPEND EP_unixodbc_CONFIGURE_OPTIONS "--enable-drivers" "--enable-driver-conf" )

set(EP_unixodbc_SPEC_FILE "unixodbc.spec")
set(EP_unixodbc_RPMBUILD_NAME "unixODBC")
set(EP_unixodbc_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "<SOURCE_DIR>/${EP_unixodbc_SPEC_FILE}" "<BINARY_DIR>")
