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

set(BELR_HEADER_FILES )
set(BELR_SOURCE_FILES_C )
set(BELR_SOURCE_FILES_CXX 
	abnf.cc
	belr.cc
	grammarbuilder.cc
	parser.cc
)

apply_compile_flags(BELR_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(BELR_SOURCE_FILES_CXX "CPP" "CXX")

if(ENABLE_STATIC)
	add_library(belr-static STATIC ${BELR_HEADER_FILES} ${BELR_SOURCE_FILES_C} ${BELR_SOURCE_FILES_CXX})
	set_target_properties(belr-static PROPERTIES OUTPUT_NAME belr)
	target_link_libraries(belr-static ${BCTOOLBOX_CORE_LIBRARIES})
endif()
if(ENABLE_SHARED)
	add_library(belr SHARED ${BELR_HEADER_FILES} ${BELR_SOURCE_FILES_C} ${BELR_SOURCE_FILES_CXX})
	if(APPLE)
		set_target_properties(belr PROPERTIES LINK_FLAGS "-stdlib=libc++")
	endif()
	set_target_properties(belr PROPERTIES VERSION ${BELR_SO_VERSION})
	target_link_libraries(belr PRIVATE ${BCTOOLBOX_CORE_LIBRARIES})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/belr.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()

if(ENABLE_STATIC)
	install(TARGETS belr-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	install(TARGETS belr EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()

install(FILES ${BELR_HEADER_FILES}
	DESTINATION include/belr
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
