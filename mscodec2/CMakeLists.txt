############################################################################
# CMakeLists.txt
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

cmake_minimum_required(VERSION 3.0)
project(MSCODEC2 VERSION 0.1 LANGUAGES C)


option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)


include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(Mediastreamer2 CONFIG REQUIRED)
find_package(bctoolbox CONFIG REQUIRED)
find_package(ortp CONFIG REQUIRED)

find_package(Codec2 REQUIRED)


include_directories(
	${MEDIASTREAMER2_INCLUDE_DIRS}
	${CODEC2_INCLUDE_DIRS}
)

set(LIBS ${MEDIASTREAMER2_LIBRARIES} ${CODEC2_LIBRARIES} ${ORTP_LIBRARIES} ${BCTOOLBOX_CORE_LIBRARIES})

set(SOURCE_FILES mscodec2.c)

if(APPLE AND NOT IOS)
   	set(MS2_FRAMEWORK_PLUGIN_DIR "${MEDIASTREAMER2_PLUGINS_LOCATION}")
else()
	set(MS2_FRAMEWORK_PLUGIN_DIR "${CMAKE_INSTALL_LIBDIR}/mediastreamer/plugins")
endif()

if(ENABLE_STATIC)
	add_library(mscodec2-static STATIC ${SOURCE_FILES})
	set_target_properties(mscodec2-static PROPERTIES OUTPUT_NAME mscodec2)
	target_link_libraries(mscodec2-static ${LIBS})
	install(TARGETS mscodec2-static
		RUNTIME DESTINATION "${MS2_FRAMEWORK_PLUGIN_DIR}"
		LIBRARY DESTINATION "${MS2_FRAMEWORK_PLUGIN_DIR}"
		ARCHIVE DESTINATION "${MS2_FRAMEWORK_PLUGIN_DIR}"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	if(NOT IOS)
		add_library(mscodec2 MODULE ${SOURCE_FILES})
	else()
		add_library(mscodec2 SHARED ${SOURCE_FILES})
	endif()
	target_link_libraries(mscodec2 ${LIBS})
	if(IOS)
		set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/build/osx/")
		set_target_properties(mscodec2 PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.mscodec2
  			MACOSX_FRAMEWORK_INFO_PLIST Info.plist.in
		) 
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/libmscodec2.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
		set_target_properties(mscodec2 PROPERTIES PREFIX "lib")
	endif()
	install(TARGETS mscodec2
		RUNTIME DESTINATION "${MS2_FRAMEWORK_PLUGIN_DIR}"
		LIBRARY DESTINATION "${MS2_FRAMEWORK_PLUGIN_DIR}"
		ARCHIVE DESTINATION "${MS2_FRAMEWORK_PLUGIN_DIR}"
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
