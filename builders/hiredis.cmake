############################################################################
# hiredis.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

#set(hiredis_filename "v0.13.3.tar.gz")
#set(EP_hiredis_URL "https://github.com/redis/hiredis/archive/${hiredis_filename}")
#set(EP_hiredis_URL_HASH "SHA1=be6f1c50fc4d649dd2924f0afecc0a1705dbe0d3")
set(EP_hiredis_GIT_REPOSITORY "https://github.com/redis/hiredis.git" CACHE STRING "hiredis repository URL")
set(EP_hiredis_GIT_TAG "010756025e8cefd1bc66c6d4ed3b1648ef6f1f95" CACHE STRING "hiredis tag to use")

set(EP_hiredis_BUILD_METHOD "rpm")
set(EP_hiredis_SPEC_FILE "hiredis.spec" )
set(EP_hiredis_CONFIG_H_FILE "${CMAKE_CURRENT_SOURCE_DIR}/builders/hiredis/${EP_hiredis_SPEC_FILE}" )

# the spec file goes into the build directory
set(EP_hiredis_PATCH_COMMAND "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" ${EP_hiredis_CONFIG_H_FILE} "<BINARY_DIR>")

# Current versions of CMake cannot download over HTTPS.. we have a speficic step that uses wget to get the archive instead of 
# using CMake's own downkoad facility.
set(EP_hiredis_CONFIGURE_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/hiredis/configure.sh.cmake)
