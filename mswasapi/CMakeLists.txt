############################################################################
# CMakeLists.txt
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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

project(MSWASAPI VERSION "1.1.1" LANGUAGES CXX)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/mediastreamer2/plugins")

set(MSWASAPI_SO_VERSION "0")

option(ENABLE_STRICT "Build with strict compile options." YES)


find_package(Mediastreamer2 5.3.0 REQUIRED)

set(SOURCE_FILES
	mswasapi.cpp
	mswasapi.h
	mswasapi_reader.cpp
	mswasapi_reader.h
	mswasapi_writer.cpp
	mswasapi_writer.h
)
set(LIBS ws2_32 Mmdevapi)
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone")
	list(APPEND LIBS phoneaudioses)
elseif( CMAKE_SYSTEM_NAME STREQUAL "WindowsStore" AND CMAKE_SYSTEM_VERSION STREQUAL "10.0" )# UWP
else()
	list(APPEND LIBS Ole32)
endif()

if(BUILD_SHARED_LIBS)
	add_library(mswasapi MODULE ${SOURCE_FILES})
else()
	add_library(mswasapi STATIC ${SOURCE_FILES})
endif()
if(ENABLE_STRICT)
	target_compile_options(mswasapi PRIVATE "/WX")
endif()
set_target_properties(mswasapi PROPERTIES PREFIX "lib")
target_link_libraries(mswasapi PRIVATE ${Mediastreamer2_TARGET} ${LIBS})
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	set_target_properties(mswasapi PROPERTIES VS_WINRT_COMPONENT TRUE)
endif()
if(MSVC)
	if(ENABLE_MICROSOFT_STORE_APP)
		message(STATUS "Build mswasapi for Microsoft Store")
		add_definitions("-DENABLE_MICROSOFT_STORE_APP")
	else()
		message(STATUS "Build mswasapi for Desktop")
	endif()
	install(FILES $<TARGET_PDB_FILE:mswasapi>
		DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		CONFIGURATIONS Debug RelWithDebInfo
	)
endif()

install(TARGETS mswasapi
	RUNTIME DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	LIBRARY DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	ARCHIVE DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
