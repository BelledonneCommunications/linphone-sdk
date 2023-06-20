############################################################################
# CMakeLists.txt
# Copyright (C) 2016-2023  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.22)

project(MSCodec2 VERSION 0.1 LANGUAGES C)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/mediastreamer2/plugins")

set(MSCODEC2_VERSION "${PROJECT_VERSION}")


include(GNUInstallDirs)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(Mediastreamer2 5.3.0 REQUIRED)
find_package(Codec2 REQUIRED)

set(SOURCE_FILES mscodec2.c)

if(BUILD_SHARED_LIBS)
	if(IOS)
		add_library(mscodec2 SHARED ${SOURCE_FILES})
	else()
		add_library(mscodec2 MODULE ${SOURCE_FILES})
	endif()
else()
	add_library(mscodec2 STATIC ${SOURCE_FILES})
endif()
target_link_libraries(mscodec2 PUBLIC ${Mediastreamer2_TARGET} PRIVATE ${Codec2_TARGET})
if(Codec2_USE_BUILD_INTERFACE)
	target_compile_definitions(mscodec2 PRIVATE CODEC2_USE_BUILD_INTERFACE)
endif()
if(BUILD_SHARED_LIBS)
	if(IOS)
		set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		set_target_properties(mscodec2 PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.mscodec2
  			MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
		) 
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES $<TARGET_PDB_FILE:mscodec2>
				DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
		set_target_properties(mscodec2 PROPERTIES PREFIX "lib")
	endif()
endif()


install(TARGETS mscodec2
	RUNTIME DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	LIBRARY DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	ARCHIVE DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
