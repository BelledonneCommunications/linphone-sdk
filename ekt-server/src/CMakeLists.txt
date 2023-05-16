############################################################################
# CMakeLists.txt
# Copyright (c) 2010-2022 Belledonne Communications SARL.
#
############################################################################
#
# This file is part of Liblinphone 
# (see https://gitlab.linphone.org/BC/public/liblinphone).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################
set(SOURCE_FILES
	ektserver.cpp
)

set(INCLUDE_DIRS )

if(ENABLE_XML2)
	list(APPEND INCLUDE_DIRS ${XML2_INCLUDE_DIRS})
endif()

set(LIBS )

if(LIBM)
	list(APPEND LIBS ${LIBM})
endif()

if(ENABLE_STATIC)
	target_include_directories(ektserver PUBLIC
		PRIVATE ${INCLUDE_DIRS}
	)
	set_target_properties(ektserver-static PROPERTIES OUTPUT_NAME ektserver)
	set_target_properties(ektserver-static PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(ektserver-static liblinphone mediastreamer ${LIBS})
	install(TARGETS ektserver-static
		ARCHIVE DESTINATION "${PLUGINS_DIR}"
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(BUILD_SHARED_LIBS)
	if(NOT IOS)
		add_library(ektserver MODULE ${SOURCE_FILES})
	else()
		add_library(ektserver SHARED ${SOURCE_FILES})
	endif()
else()
	add_library(ektserver STATIC ${SOURCE_FILES})

endif()

target_include_directories(ektserver PUBLIC
	$<INSTALL_INTERFACE:include/>
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/>
	PRIVATE ${INCLUDE_DIRS}
)

target_link_libraries(ektserver PUBLIC liblinphone mediastreamer2 ${LIBS})
set_target_properties(ektserver PROPERTIES LINKER_LANGUAGE CXX)
if(APPLE)
	if(IOS)
		set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		set_target_properties(ektserver 	PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.ektserver
			MACOSX_FRAMEWORK_INFO_PLIST Info.plist.in
		)
	else()
		set(MIN_OS ${CMAKE_OSX_DEPLOYMENT_TARGET})
	endif()
endif()
if(MSVC)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		install(FILES $<TARGET_PDB_FILE:ektserver>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	endif()
	if(NOT IOS)
		set_target_properties(ektserver PROPERTIES PREFIX "lib")
	endif()
endif()
install(TARGETS ektserver
		RUNTIME DESTINATION ${PLUGINS_DIR}
		LIBRARY DESTINATION ${PLUGINS_DIR}
		ARCHIVE DESTINATION ${PLUGINS_DIR}
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

