############################################################################
# xml2.cmake
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

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(XML2_FILENAME "xml2-v2.8.0-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${XML2_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${XML2_FILENAME}" STATUS XML2_FILENAME_STATUS)
	list(GET XML2_FILENAME_STATUS 0 XML2_DOWNLOAD_STATUS)
	if(NOT XML2_DOWNLOAD_STATUS)
		set(XML2_PREBUILT 1)
	endif()
endif()

if(XML2_PREBUILT)
	lcb_url("${CMAKE_CURRENT_BINARY_DIR}/${XML2_FILENAME}")
	lcb_build_method("prebuilt")
else()
	lcb_git_repository("git://git.linphone.org/libxml2")
	lcb_git_tag("v2.8.0")
	lcb_external_source_paths("libxml2" "xml2" "externals/libxml2")
	lcb_may_be_found_on_system(YES)
	lcb_ignore_warnings(YES)

	lcb_patch_command(
		"${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/CMakeLists.txt" "<SOURCE_DIR>"
		"COMMAND"
		"${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/config.h.cmake" "<SOURCE_DIR>"
	)
	if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone")
		lcb_patch_command("COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/windowsphone_port.h" "<SOURCE_DIR>")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		lcb_patch_command("COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/universal_windows_port.h" "<SOURCE_DIR>")
	endif()
endif()
