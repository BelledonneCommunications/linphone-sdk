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

set(MSWASAPI_SO_VERSION "0")

option(ENABLE_STRICT "Build with strict compile options." YES)


list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(BcToolbox)
if(NOT BCTOOLBOX_FOUND)
	find_package(bctoolbox CONFIG REQUIRED COMPONENTS core)
endif()
find_package(Ortp)
if(NOT ORTP_FOUND)
	find_package(ortp CONFIG REQUIRED)
endif()
find_package(Mediastreamer2)
if(NOT MEDIASTREAMER2_FOUND)
	find_package(Mediastreamer2 CONFIG REQUIRED)
endif()

include_directories(
	${MEDIASTREAMER2_INCLUDE_DIRS}
	${ORTP_INCLUDE_DIRS}
	${BCTOOLBOX_CORE_INCLUDE_DIRS}
)

set(SOURCE_FILES
	mswasapi.cpp
	mswasapi.h
	mswasapi_reader.cpp
	mswasapi_reader.h
	mswasapi_writer.cpp
	mswasapi_writer.h
)
set(LIBS ${MEDIASTREAMER2_LIBRARIES} ${ORTP_LIBRARIES} ${BCTOOLBOX_CORE_LIBRARIES} ws2_32)
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone")
	list(APPEND LIBS phoneaudioses)
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
target_link_libraries(mswasapi ${LIBS})
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
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
	install(FILES $<TARGET_PDB_FILE:mswasapi>
		DESTINATION "${MEDIASTREAMER2_PLUGINS_LOCATION}"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()

install(TARGETS mswasapi
	RUNTIME DESTINATION "${MEDIASTREAMER2_PLUGINS_LOCATION}"
	LIBRARY DESTINATION "${MEDIASTREAMER2_PLUGINS_LOCATION}"
	ARCHIVE DESTINATION "${MEDIASTREAMER2_PLUGINS_LOCATION}"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
