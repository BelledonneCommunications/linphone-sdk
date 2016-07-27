############################################################################
# additional_steps.cmake
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

# Add external project to build the Python module
if(NOT PACKAGE_NAME)
	set(PACKAGE_NAME "linphone")
endif()

linphone_builder_apply_flags()
linphone_builder_set_ep_directories(pylinphone)
linphone_builder_expand_external_project_vars()

ExternalProject_Add(TARGET_pylinphone
	DEPENDS TARGET_linphone_builder
	TMP_DIR ${ep_tmp}
	BINARY_DIR ${ep_build}
	DOWNLOAD_COMMAND ""
	PATCH_COMMAND "${CMAKE_COMMAND}" "-E" "copy_directory" "${CMAKE_CURRENT_LIST_DIR}" "<SOURCE_DIR>"
		"COMMAND" "${CMAKE_COMMAND}" "-E" "copy" "${CMAKE_CURRENT_LIST_DIR}/../../cmake/FindXML2.cmake" "<SOURCE_DIR>"
	CMAKE_GENERATOR ${CMAKE_GENERATOR}
	CMAKE_ARGS ${LINPHONE_BUILDER_EP_ARGS} -DPACKAGE_NAME=${PACKAGE_NAME} -DLINPHONE_SOURCE_DIR=${EP_linphone_SOURCE_DIR} -DENABLE_FFMPEG:BOOL=${ENABLE_FFMPEG} -DENABLE_OPENH264:BOOL=${ENABLE_OPENH264} -DENABLE_WASAPI:BOOL=${ENABLE_WASAPI}
)
