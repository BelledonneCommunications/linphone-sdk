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

set(SOURCE_FILES
	bzrtp.c
	cryptoUtils.c
	packetParser.c
	pgpwords.c
	stateMachine.c
	zidCache.c
)

add_definitions(
	-DBCTBX_LOG_DOMAIN="bzrtp"
)

if(POLARSSL_FOUND)
	list(APPEND SOURCE_FILES cryptoPolarssl.c)
elseif(MBEDTLS_FOUND)
	list(APPEND SOURCE_FILES cryptoMbedtls.c)
endif()

bc_apply_compile_flags(SOURCE_FILES STRICT_OPTIONS_CPP)

set(INCLUDE_DIRS )
set(LIBS )

if(SQLITE3_FOUND)
	list(APPEND INCLUDE_DIRS ${SQLITE3_INCLUDE_DIRS})
	list(APPEND LIBS $<BUILD_INTERFACE:${SQLITE3_LIBRARIES}> $<INSTALL_INTERFACE:${SQLITE3_LIBRARIES}>)
endif()

if(XML2_FOUND)
	list(APPEND INCLUDE_DIRS ${XML2_INCLUDE_DIRS})
	list(APPEND LIBS $<BUILD_INTERFACE:${XML2_LIBRARIES}>  $<INSTALL_INTERFACE:${XML2_LIBRARIES}>)
endif()

if(ENABLE_STATIC)
	add_library(bzrtp STATIC ${SOURCE_FILES})
	set_target_properties(bzrtp PROPERTIES OUTPUT_NAME bzrtp)
	target_include_directories(bzrtp INTERFACE
		$<INSTALL_INTERFACE:include>
		PRIVATE ${INCLUDE_DIRS})
	target_link_libraries(bzrtp PUBLIC bctoolbox ${LIBS})
	install(TARGETS bzrtp EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(bzrtp SHARED ${SOURCE_FILES})
	target_compile_definitions(bzrtp PRIVATE "-DBZRTP_EXPORTS")
	set_target_properties(bzrtp PROPERTIES VERSION 0)
	target_include_directories(bzrtp INTERFACE
		$<INSTALL_INTERFACE:include>
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
		PRIVATE ${INCLUDE_DIRS}
	)
	target_link_libraries(bzrtp PUBLIC bctoolbox ${LIBS})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bzrtp.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	install(TARGETS bzrtp EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
