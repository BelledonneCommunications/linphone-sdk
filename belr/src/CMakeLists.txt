############################################################################
# CMakeLists.txt
# Copyright (C) 2015-2023  Belledonne Communications, Grenoble France
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

set(LIBS )

set(BELR_HEADER_FILES )
set(BELR_SOURCE_FILES_CXX
	abnf.cpp
	belr.cpp
	grammarbuilder.cpp
	parser.cpp
	binarystream.cpp
)

bc_apply_compile_flags(BELR_SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	list(APPEND LIBS ws2_32 Iphlpapi)
endif()

add_library(belr ${BELR_HEADER_FILES} ${BELR_SOURCE_FILES_CXX})
set_target_properties(belr PROPERTIES VERSION ${BELR_SO_VERSION})
target_include_directories(belr INTERFACE
	$<INSTALL_INTERFACE:include>
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
target_compile_definitions(belr PRIVATE "BCTBX_LOG_DOMAIN=\"belr\"")
target_link_libraries(belr PUBLIC ${BCToolbox_TARGET} PRIVATE ${LIBS})

if(BUILD_SHARED_LIBS)
	target_compile_definitions(belr PRIVATE "BELR_EXPORTS")
	if(APPLE)
		set_target_properties(belr PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.belr
			MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
			PUBLIC_HEADER "${BELR_HEADER_FILES}"
		)
	endif()
	if(MSVC)
		install(FILES $<TARGET_PDB_FILE:belr>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
	endif()
else()
	target_compile_definitions(belr PUBLIC "BELR_STATIC")
	set_target_properties(belr PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

install(TARGETS belr EXPORT ${PROJECT_NAME}Targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES ${BELR_HEADER_FILES}
	DESTINATION include/belr
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
