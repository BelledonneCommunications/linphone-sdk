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

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(hiredis LANGUAGES C VERSION 0.13.3)

option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)

include(GNUInstallDirs)

find_package(BcToolbox 0.0.3 REQUIRED)

set(HEADER_FILES
	async.h
	hiredis.h
	read.h
	sds.h
)

set(SOURCE_FILES
	async.c
	hiredis.c
	net.c
	read.c
	sds.c
)

if(ENABLE_STATIC)
	add_library(hiredis-static STATIC ${SOURCE_FILES})
	set_target_properties(hiredis-static PROPERTIES OUTPUT_NAME hiredis)
	install(TARGETS hiredis-static
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(hiredis SHARED ${SOURCE_FILES})
	set_target_properties(hiredis PROPERTIES VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
	install(TARGETS hiredis
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()


install(FILES ${HEADER_FILES}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hiredis
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
install(DIRECTORY adapters DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/hiredis)

add_subdirectory(build)

