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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

if(LINPHONE_BUILDER_PREBUILT_URL)
	set(EP_xml2_FILENAME "xml2-v2.8.0-${LINPHONE_BUILDER_ARCHITECTURE}.zip")
	file(DOWNLOAD "${LINPHONE_BUILDER_PREBUILT_URL}/${EP_xml2_FILENAME}" "${CMAKE_CURRENT_BINARY_DIR}/${EP_xml2_FILENAME}" STATUS EP_xml2_FILENAME_STATUS)
	list(GET EP_xml2_FILENAME_STATUS 0 EP_xml2_DOWNLOAD_STATUS)
	if(NOT EP_xml2_DOWNLOAD_STATUS)
		set(EP_xml2_PREBUILT 1)
	endif()
endif()

if(EP_xml2_PREBUILT)
	set(EP_xml2_URL "${CMAKE_CURRENT_BINARY_DIR}/${EP_xml2_FILENAME}")
	set(EP_xml2_BUILD_METHOD "prebuilt")
else()
	set(EP_xml2_GIT_REPOSITORY "git://git.linphone.org/libxml2" CACHE STRING "xml2 repository URL")
	set(EP_xml2_GIT_TAG "v2.8.0" CACHE STRING "xml2 tag to use")
	set(EP_xml2_EXTERNAL_SOURCE_PATHS "libxml2" "xml2" "externals/libxml2")

	set(EP_xml2_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
	set(EP_xml2_PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/CMakeLists.txt" "<SOURCE_DIR>")
	list(APPEND EP_xml2_PATCH_COMMAND "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/config.h.cmake" "<SOURCE_DIR>")
	if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone")
		list(APPEND EP_xml2_PATCH_COMMAND "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/windowsphone_port.h" "<SOURCE_DIR>")
	elseif(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		list(APPEND EP_xml2_PATCH_COMMAND "COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_SOURCE_DIR}/builders/xml2/universal_windows_port.h" "<SOURCE_DIR>")
	endif()
endif()
