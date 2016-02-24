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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
if(POLARSSL_FOUND)
	list(APPEND SOURCE_FILES cryptoPolarssl.c)
elseif(MBEDTLS_FOUND)
	list(APPEND SOURCE_FILES cryptoMbedtls.c)
endif()

apply_compile_flags(SOURCE_FILES "CPP")

add_library(bzrtp-objects OBJECT ${SOURCE_FILES})
set_target_properties(bzrtp-objects PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(ENABLE_STATIC)
	add_library(bzrtp-static STATIC $<TARGET_OBJECTS:bzrtp-objets>)
	set_target_properties(bzrtp-static PROPERTIES OUTPUT_NAME bzrtp)
	target_link_libraries(bzrtp-static ${LIBS})
	install(TARGETS bzrtp-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(bzrtp SHARED $<TARGET_OBJECTS:bzrtp-objects>)
	set_target_properties(bzrtp PROPERTIES VERSION 0)
	target_link_libraries(bzrtp ${LIBS})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/bzrtp.pdb
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
