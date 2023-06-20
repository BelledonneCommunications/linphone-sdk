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

set(SOURCE_FILES
	msopenh264.cpp
	msopenh264dec.cpp
	msopenh264enc.cpp
)

if(BUILD_SHARED_LIBS)
	if(APPLE)
		add_library(msopenh264 SHARED ${SOURCE_FILES})
	else()
		add_library(msopenh264 MODULE ${SOURCE_FILES})
	endif()
else()
	add_library(msopenh264 STATIC ${SOURCE_FILES})
endif()
target_compile_options(msopenh264 PRIVATE "-std=c++11")
target_compile_definitions(msopenh264 PRIVATE "HAVE_CONFIG_H")
if(ENABLE_DECODER)
	target_compile_definitions(msopenh264 PRIVATE "DOPENH264_DECODER_ENABLED")
endif()
target_include_directories(msopenh264 PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${PROJECT_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(msopenh264 PRIVATE ${Mediastreamer2_TARGET} ${OpenH264_TARGET})

if(BUILD_SHARED_LIBS)
	if(APPLE)
		if(IOS)
			set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		else()
			set(MIN_OS ${CMAKE_OSX_DEPLOYMENT_TARGET})
		endif()

		set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/build/osx/")
		set_target_properties(msopenh264 PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.msopenh264
			MACOSX_FRAMEWORK_INFO_PLIST Info.plist.in
		) 
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES $<TARGET_PDB_FILE:msopenh264>
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
		set_target_properties(msopenh264 PROPERTIES PREFIX "lib")
		if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
			set_target_properties(msopenh264 PROPERTIES VS_WINRT_COMPONENT TRUE)
		endif()
	
	endif()
endif()


install(TARGETS msopenh264
	RUNTIME DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	LIBRARY DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	ARCHIVE DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
