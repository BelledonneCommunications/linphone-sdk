############################################################################
# libodb.cmake
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

set(libodb_filename "libodb-2.3.0.tar.bz2")
set(EP_libodb_URL "http://www.codesynthesis.com/download/odb/2.3/${libodb_filename}")
set(EP_libodb_URL_HASH "SHA1=eebc7fa706bc598a80439d1d6a798430fcfde23b")

set(EP_libodb_BUILD_METHOD "rpm")
set(EP_libodb_SPEC_FILE "libodb.spec" )
set(EP_libodb_CONFIG_H_FILE "${CMAKE_CURRENT_SOURCE_DIR}/builders/libodb/${EP_libodb_SPEC_FILE}" )
set(EP_libodb_DEPENDENCIES EP_odb)

#create source dir and copy the tar.gz inside
set(EP_libodb_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "make_directory" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_libodb_PATCH_COMMAND ${EP_libodb_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${LINPHONE_BUILDER_WORK_DIR}/Download/EP_libodb/${libodb_filename}" "${LINPHONE_BUILDER_WORK_DIR}/rpmbuild/SOURCES/")
set(EP_libodb_PATCH_COMMAND ${EP_libodb_PATCH_COMMAND} "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" ${EP_libodb_CONFIG_H_FILE} "<BINARY_DIR>")
