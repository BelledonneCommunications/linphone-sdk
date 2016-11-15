############################################################################
# sqlite3.cmake
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

lcb_url("http://www.sqlite.org/2014/sqlite-amalgamation-3080702.zip")
lcb_url_hash("MD5=10587262e4381358b707df75392c895f")
lcb_external_source_paths("externals/sqlite3")
lcb_may_be_found_on_system(YES)
lcb_ignore_warnings(YES)

lcb_patch_command("${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/sqlite3/CMakeLists.txt" "<SOURCE_DIR>")
if(WIN32)
	lcb_patch_command(
		"COMMAND"
		"${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/sqlite3/sqlite3.def" "<SOURCE_DIR>"
	)
endif()
