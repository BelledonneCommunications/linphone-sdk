############################################################################
# hiredis.cmake
# Copyright (C) 2015-2018  Belledonne Communications, Grenoble France
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

lcb_git_repository("https://github.com/redis/hiredis.git")
lcb_external_source_paths("externals/hiredis")
lcb_package_source(YES)
lcb_spec_file("hiredis.spec")

lcb_dependencies(bctoolbox)

lcb_patch_command("COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/hiredis/CMakeLists.txt" "<SOURCE_DIR>")
lcb_patch_command("COMMAND" "${CMAKE_COMMAND}" "-E" "make_directory" "<SOURCE_DIR>/build/")
lcb_patch_command("COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/hiredis/build/CMakeLists.txt" "<SOURCE_DIR>/build/")
lcb_patch_command("COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/hiredis/build/hiredis.spec.cmake" "<SOURCE_DIR>/build/")

