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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

cmake_minimum_required(VERSION 3.0)
project(MSWASAPI CXX)


set(MSWASAPI_MAJOR_VERSION "1")
set(MSWASAPI_MINOR_VERSION "1")
set(MSWASAPI_MICRO_VERSION "0")
set(MSWASAPI_VERSION "${MSWASAPI_MAJOR_VERSION}.${MSWASAPI_MINOR_VERSION}.${MSWASAPI_MICRO_VERSION}")
set(MSWASAPI_SO_VERSION "0")

option(ENABLE_SHARED "Build shared library." YES)
option(ENABLE_STATIC "Build static library." YES)
option(ENABLE_STRICT "Build with strict compile options." YES)


macro(apply_compile_flags SOURCE_FILES)
	if(${SOURCE_FILES})
		set(options "")
		foreach(a ${ARGV})
			if(STRICT_OPTIONS_${a})
				string(REPLACE ";" " " options_${a} "${STRICT_OPTIONS_${a}}")
				set(options "${options} ${options_${a}}")
			endif()
		endforeach()
		if(options)
			set_source_files_properties(${${SOURCE_FILES}} PROPERTIES COMPILE_FLAGS "${options}")
		endif()
	endif()
endmacro()


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_ms2_CONFIG_DIR}/Mediastreamer2Config.cmake")
	include("${EP_ortp_CONFIG_DIR}/ORTPConfig.cmake")
	include("${EP_bctoolbox_CONFIG_DIR}/BcToolboxConfig.cmake")
else()
	find_package(Mediastreamer2 REQUIRED)
	find_package(ORTP REQUIRED)
	find_package(BcToolbox REQUIRED COMPONENTS core)
endif()


include_directories(
	${MEDIASTREAMER2_INCLUDE_DIRS}
	${ORTP_INCLUDE_DIRS}
	${BCTOOLBOX_CORE_INCLUDE_DIRS}
)

set(STRICT_OPTIONS_CPP )
if(ENABLE_STRICT)
	list(APPEND STRICT_OPTIONS_CPP "/WX")
endif()

set(SOURCE_FILES
	mswasapi.cpp
	mswasapi.h
	mswasapi_reader.cpp
	mswasapi_reader.h
	mswasapi_writer.cpp
	mswasapi_writer.h
)
apply_compile_flags(SOURCE_FILES "CPP")
set(LIBS ${MEDIASTREAMER2_LIBRARIES} ${ORTP_LIBRARIES} ${BCTOOLBOX_CORE_LIBRARIES} ws2_32)
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
