############################################################################
# CMakeLists.txt
# Copyright (C) 2017  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.0)
project(PACKAGE_SOURCE_CHECK)

include(ExternalProject)

set(DEPENDENCIES)
set(TAR_FILES)
string(REPLACE " " ";" LINPHONE_BUILDER_SOURCE_PACKAGE_BUILDERS ${LINPHONE_BUILDER_SOURCE_PACKAGE_BUILDERS})
foreach(BUILDER ${LINPHONE_BUILDER_SOURCE_PACKAGE_BUILDERS})
	file(GLOB TAR_FILE "${LINPHONE_BUILDER_WORK_DIR}/Build/${BUILDER}/*.tar.gz")
	list(APPEND TAR_FILES "${TAR_FILE}")

	if(DEPENDENCIES)
		set(DEPENDENCY_ARGS "DEPENDS" ${DEPENDENCIES})
	else()
		set(DEPENDENCY_ARGS)
	endif()
	ExternalProject_Add(PS_${BUILDER}
		${DEPENDENCY_ARGS}
		URL "${TAR_FILE}"
		CMAKE_GENERATOR ${GENERATOR}
		CMAKE_ARGS -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH} -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
	)
	list(APPEND DEPENDENCIES "PS_${BUILDER}")
endforeach()

install(FILES ${TAR_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}")
