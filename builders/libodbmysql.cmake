############################################################################
# libodbmysql.cmake
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

set(libodbmysql_filename "libodb-mysql-2.3.0.tar.bz2")
set(EP_libodbmysql_URL "http://www.codesynthesis.com/download/odb/2.3/${libodbmysql_filename}")
set(EP_libodbmysql_URL_HASH "SHA1=18adaa5535015e3471a5d205e44df42f0e3a3d37")

set(EP_libodbmysql_BUILD_METHOD "rpm")
set(EP_libodbmysql_SPEC_FILE "libodbmysql.spec" )
set(EP_libodbmysql_CONFIG_H_FILE "${CMAKE_CURRENT_SOURCE_DIR}/builders/libodbmysql/${EP_libodbmysql_SPEC_FILE}" )
set(EP_libodbmysql_DEPENDENCIES EP_libodb)
set(EP_libodbmysql_RPMBUILD_NAME "libodb-mysql")

#create source dir and copy the tar.gz inside
set(EP_libodbmysql_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_libodbmysql_PATCH_COMMAND ${EP_libodbmysql_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${LINPHONE_BUILDER_WORK_DIR}/Download/EP_libodbmysql/${libodbmysql_filename}" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_libodbmysql_PATCH_COMMAND ${EP_libodbmysql_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" ${EP_libodbmysql_CONFIG_H_FILE} "<BINARY_DIR>")
