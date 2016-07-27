############################################################################
# CMakeLists.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

apply_compile_flags(BELCARD_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(BELCARD_SOURCE_FILES_CXX "CPP" "CXX")

if(ENABLE_STATIC)
	add_library(belcard-static STATIC ${BELCARD_HEADER_FILES} ${BELCARD_SOURCE_FILES_C} ${BELCARD_SOURCE_FILES_CXX})
	set_target_properties(belcard-static PROPERTIES OUTPUT_NAME belcard)
	target_link_libraries(belcard-static ${BELR_LIBRARIES} ${BCTOOLBOX_CORE_LIBRARIES})
endif()
if(ENABLE_SHARED)
	add_library(belcard SHARED ${BELCARD_HEADER_FILES} ${BELCARD_SOURCE_FILES_C} ${BELCARD_SOURCE_FILES_CXX})
	if(APPLE)
		set_target_properties(belcard PROPERTIES LINK_FLAGS "-stdlib=libc++")
	endif()
	set_target_properties(belcard PROPERTIES VERSION ${BELCARD_SO_VERSION})
	target_link_libraries(belcard PRIVATE ${BELR_LIBRARIES} ${BCTOOLBOX_CORE_LIBRARIES})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/belcard.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()

if(ENABLE_STATIC)
	install(TARGETS belcard-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	install(TARGETS belcard EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()

install(FILES ${BELCARD_HEADER_FILES}
	DESTINATION include/belcard
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
