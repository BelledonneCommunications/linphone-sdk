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

set(BELCARD_HEADER_FILES )
set(BELCARD_SOURCE_FILES_C )
set(BELCARD_SOURCE_FILES_CXX
	belcard_addressing.cpp
	belcard_communication.cpp
	belcard_explanatory.cpp
	belcard_geographical.cpp
	belcard_organizational.cpp
	belcard_parser.cpp
	belcard_rfc6474.cpp
	belcard_utils.cpp
	belcard_calendar.cpp
	belcard.cpp
	belcard_general.cpp
	belcard_identification.cpp
	belcard_params.cpp
	belcard_property.cpp
	belcard_security.cpp
	vcard_grammar.cpp
)

set(VCARD_GRAMMAR_FILES
	vcard_grammar
	vcard3_grammar
)

bc_apply_compile_flags(BELCARD_SOURCE_FILES_C STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(BELCARD_SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

add_library(belcard ${BELCARD_HEADER_FILES} ${BELCARD_SOURCE_FILES_C} ${BELCARD_SOURCE_FILES_CXX} ${VCARD_GRAMMAR_FILES})
set_target_properties(belcard PROPERTIES VERSION ${BELCARD_SO_VERSION})
target_include_directories(belcard INTERFACE
  $<INSTALL_INTERFACE:include>
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
target_link_libraries(belcard PUBLIC ${Belr_TARGET} PRIVATE ${BCToolbox_TARGET})

if(BUILD_SHARED_LIBS)
	if(APPLE)
		set_target_properties(belcard PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.belcard
			MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
			RESOURCE "${VCARD_GRAMMAR_FILES}"
			PUBLIC_HEADER "${BELCARD_HEADER_FILES}"
		)
	endif()
	if(MSVC)
		install(FILES $<TARGET_PDB_FILE:belcard>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
	endif()
endif()

install(TARGETS belcard EXPORT ${PROJECT_NAME}Targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES ${BELCARD_HEADER_FILES}
	DESTINATION include/belcard
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)

install(FILES ${VCARD_GRAMMAR_FILES}
	DESTINATION "${CMAKE_INSTALL_DATADIR}/belr/grammars"
)
