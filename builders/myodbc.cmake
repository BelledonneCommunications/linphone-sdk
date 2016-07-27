############################################################################
# myodbc.cmake
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

set(EP_myodbc_GIT_REPOSITORY "git://git.linphone.org/myodbc.git" CACHE STRING "myodbc repository URL")
set(EP_myodbc_GIT_TAG_LATEST "master" CACHE STRING "myodbc tag to use when compiling latest version")
set(EP_myodbc_GIT_TAG "4bc36b6d5d6e2ca67d1bc5a3bab7728853f2a683" CACHE STRING "myodbc tag to use")

set(EP_myodbc_DEPENDENCIES EP_unixodbc)
set(EP_myodbc_SPEC_FILE "scripts/myodbc3.spec")
set(EP_myodbc_CONFIG_H_FILE "scripts/myodbc3.spec")
set(EP_myodbc_CONFIGURE_EXTRA_CMD "cd scripts && make && cd -")
set(EP_myodbc_RPMBUILD_NAME "mysql-connector")

set(EP_myodbc_CMAKE_OPTIONS "-DWITH_UNIXODBC=1")
