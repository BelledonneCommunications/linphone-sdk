############################################################################
# CMakeLists.txt
# Copyright (C) 2010-2023 Belledonne Communications, Grenoble France
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

set(BCToolbox_CMAKE_DIR "${PROJECT_SOURCE_DIR}/cmake")
set(BCToolbox_CMAKE_UTILS "${BCToolbox_CMAKE_DIR}/BCToolboxCMakeUtils.cmake")
include("${BCToolbox_CMAKE_UTILS}")

if(NOT CPACK_PACKAGE_NAME)
	string(TOLOWER "${PROJECT_NAME}" LOWERCASE_PROJECT_NAME)
	set(CPACK_PACKAGE_NAME "${LOWERCASE_PROJECT_NAME}")
endif()

set(CPACK_SOURCE_IGNORE_FILES
	"^${PROJECT_BINARY_DIR}"
	"^${PROJECT_SOURCE_DIR}/.git*"
)


bc_make_package_source_target()
