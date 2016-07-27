############################################################################
# odb.cmake
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

set(odb_filename "odb-2.3.0.tar.bz2")
set(EP_odb_URL "http://www.codesynthesis.com/download/odb/2.3/${odb_filename}")
set(EP_odb_URL_HASH "SHA1=fe18c7154085afec23c18aa940f168de7068f6f3")
set(EP_odb_BUILD_METHOD "rpm")

set(EP_odb_SPEC_FILE "odb.spec" )
set(EP_odb_CONFIG_H_FILE "${CMAKE_CURRENT_SOURCE_DIR}/builders/odb/${EP_odb_SPEC_FILE}" )
set(EP_odb_RPMBUILD_NAME "odb_")

#create source dir and copy the tar.gz inside
set(EP_odb_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_odb_PATCH_COMMAND ${EP_odb_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${LINPHONE_BUILDER_WORK_DIR}/Download/EP_odb/${odb_filename}" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_odb_PATCH_COMMAND ${EP_odb_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" ${EP_odb_CONFIG_H_FILE} "<BINARY_DIR>")

set(EP_odb_CONFIGURE_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/odb/configure.sh.cmake)
set(EP_odb_BUILD_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/odb/build.sh.cmake)
set(EP_odb_INSTALL_COMMAND_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/builders/odb/install.sh.cmake)
