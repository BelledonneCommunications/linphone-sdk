############################################################################
# soci.cmake
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

set(soci_filename "soci-3.2.3.tar.gz")
set(EP_soci_URL "${CMAKE_CURRENT_SOURCE_DIR}/builders/soci/${soci_filename}")
set(EP_soci_URL_HASH "SHA1=5e527cf5c1740198fa706fc8821af45b34867ee1")

set(EP_soci_BUILD_METHOD "rpm")
set(EP_soci_SPEC_FILE "soci.spec" )
set(EP_soci_CONFIG_H_FILE "${CMAKE_CURRENT_SOURCE_DIR}/builders/soci/${EP_soci_SPEC_FILE}" )
set(EP_soci_RPMBUILD_OPTIONS "--without postgresql --without sqlite3 --without odbc --with mysql --without oracle --define 'soci_patch ${CMAKE_CURRENT_SOURCE_DIR}/builders/soci/soci_libdir.patch'")

#create source dir and copy the tar.gz inside
set(EP_soci_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_soci_PATCH_COMMAND ${EP_soci_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${EP_soci_URL}" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_soci_PATCH_COMMAND ${EP_soci_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/soci/soci_libdir.patch" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_soci_PATCH_COMMAND ${EP_soci_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" ${EP_soci_CONFIG_H_FILE} "<BINARY_DIR>")

# no configure needed for soci
set(EP_soci_CONFIGURE_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/soci/configure.sh.cmake)
