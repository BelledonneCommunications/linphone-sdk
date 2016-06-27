############################################################################
# CMakeLists.txt
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

cmake_minimum_required(VERSION 3.0)
project(MSWASAPI CXX)


option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_ms2_CONFIG_DIR}/Mediastreamer2Config.cmake")
else()
	find_package(Mediastreamer2 REQUIRED)
endif()


include_directories(
	${MEDIASTREAMER2_INCLUDE_DIRS}
)

set(SOURCE_FILES
	mswasapi.cpp
	mswasapi.h
	mswasapi_reader.cpp
	mswasapi_reader.h
	mswasapi_writer.cpp
	mswasapi_writer.h
)
set(LIBS ${MEDIASTREAMER2_LIBRARIES} ws2_32)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsPhone")
	list(APPEND LIBS phoneaudioses)
endif()

if(ENABLE_STATIC)
	add_library(mswasapi-static STATIC ${SOURCE_FILES})
	set_target_properties(mswasapi-static PROPERTIES OUTPUT_NAME mswasapi)
	target_link_libraries(mswasapi-static ${LIBS})
	install(TARGETS mswasapi-static
		ARCHIVE DESTINATION lib/mediastreamer/plugins
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(mswasapi MODULE ${SOURCE_FILES})
	set_target_properties(mswasapi PROPERTIES PREFIX "lib")
	target_link_libraries(mswasapi ${LIBS})
	if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		set_target_properties(mswasapi PROPERTIES VS_WINRT_COMPONENT TRUE)
	endif()
	if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libmswasapi.pdb
			DESTINATION bin
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	endif()
	install(TARGETS mswasapi
		RUNTIME DESTINATION lib/mediastreamer/plugins
		LIBRARY DESTINATION lib/mediastreamer/plugins
		ARCHIVE DESTINATION lib/mediastreamer/plugins
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
