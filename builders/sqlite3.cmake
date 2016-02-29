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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(EP_sqlite3_URL "http://www.sqlite.org/2014/sqlite-amalgamation-3080702.zip")
set(EP_sqlite3_URL_HASH "MD5=10587262e4381358b707df75392c895f")

set(EP_sqlite3_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/sqlite3/CMakeLists.txt" "<SOURCE_DIR>")
if(WIN32)
	list(APPEND EP_sqlite3_PATCH_COMMAND "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/sqlite3/sqlite3.def" "<SOURCE_DIR>")
endif()
set(EP_sqlite3_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
