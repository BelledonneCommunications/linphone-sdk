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

set(LIBS
	${MEDIASTREAMER2_LIBRARIES}
	${ORTP_LIBRARIES}
	${OPENH264_LIBRARIES}
	${BCTOOLBOX_CORE_LIBRARIES}
)

set(SOURCE_FILES
	msopenh264.cpp
	msopenh264dec.cpp
	msopenh264enc.cpp
)

if(ENABLE_DECODER)
	add_definitions(-DOPENH264_DECODER_ENABLED)
endif()

set(MS2_PLUGINS_DIR "${MEDIASTREAMER2_PLUGINS_LOCATION}")

if(ENABLE_STATIC)
	add_library(msopenh264-static STATIC ${SOURCE_FILES})
	set_target_properties(msopenh264-static PROPERTIES OUTPUT_NAME msopenh264)
	target_link_libraries(msopenh264-static ${LIBS})
	install(TARGETS msopenh264-static
		ARCHIVE DESTINATION "${MS2_PLUGINS_DIR}"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	if(NOT APPLE)
		add_library(msopenh264 MODULE ${SOURCE_FILES})
	else()
		add_library(msopenh264 SHARED ${SOURCE_FILES})
	endif()
	target_link_libraries(msopenh264 ${LIBS})
	if(APPLE)
		if(IOS)
			set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		else()
			set(MIN_OS ${CMAKE_OSX_DEPLOYMENT_TARGET})
		endif()

		set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/build/osx/")
		set_target_properties(msopenh264 PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.msopenh264
  			MACOSX_FRAMEWORK_INFO_PLIST Info.plist.in
		) 
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libmsopenh264.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
		set_target_properties(msopenh264 PROPERTIES PREFIX "lib")
	endif()
	install(TARGETS msopenh264
		RUNTIME DESTINATION "${MS2_PLUGINS_DIR}"
		LIBRARY DESTINATION "${MS2_PLUGINS_DIR}"
		ARCHIVE DESTINATION "${MS2_PLUGINS_DIR}"
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
