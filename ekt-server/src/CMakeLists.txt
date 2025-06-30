############################################################################
# CMakeLists.txt
# Copyright (c) 2010-2023 Belledonne Communications SARL.
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
	ekt-server-main.cpp
	ekt-server-main.h
	ektserver.cpp
	server-ekt-manager.cpp
	server-ekt-manager.h
)

set(INCLUDE_DIRS )
set(LIBS )

if(LIBM)
	list(APPEND LIBS ${LIBM})
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

target_link_libraries(ektserver PUBLIC ${LibLinphoneCxx_TARGET} ${LibLinphone_TARGET} ${Mediastreamer2_TARGET} ${BCToolbox_TARGET} ${LIBS})
set_target_properties(ektserver PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(ektserver PROPERTIES OUTPUT_NAME "linphone_ektserver")
if(APPLE)
	if(IOS)
		set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		set_target_properties(ektserver PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.ektserver
			MACOSX_FRAMEWORK_INFO_PLIST Info.plist.in
		)
	else()
		set(MIN_OS ${CMAKE_OSX_DEPLOYMENT_TARGET})
	endif()
endif()
if(MSVC)
	install(FILES $<TARGET_PDB_FILE:ektserver>
		DESTINATION ${CMAKE_INSTALL_BINDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		CONFIGURATIONS Debug RelWithDebInfo
	)
	set_target_properties(ektserver PROPERTIES PREFIX "lib")
endif()
install(TARGETS ektserver
	COMPONENT plugin-ektserver
	RUNTIME DESTINATION ${LibLinphone_PLUGINS_DIR}
	LIBRARY DESTINATION ${LibLinphone_PLUGINS_DIR}
	ARCHIVE DESTINATION ${LibLinphone_PLUGINS_DIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

