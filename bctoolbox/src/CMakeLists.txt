############################################################################
# CMakeLists.txt
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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


set(BCTOOLBOX_SOURCE_FILES
	crypto.c
)


if(ENABLE_STATIC)
	add_library(bctoolbox STATIC ${BCTOOLBOX_HEADER_FILES} ${BCTOOLBOX_SOURCE_FILES})
else()
	add_library(bctoolbox SHARED ${BCTOOLBOX_HEADER_FILES} ${BCTOOLBOX_SOURCE_FILES})
	set_target_properties(bctoolbox PROPERTIES VERSION 0.0.0 SOVERSION 0)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/bctoolbox.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()

list(APPEND INCLUDES ${POLARSSL_INCLUDE_DIR})
list(APPEND LIBS ${POLARSSL_LIBRARIES})


target_include_directories(bctoolbox PUBLIC ${INCLUDES})
target_link_libraries(bctoolbox ${LIBS})

install(TARGETS bctoolbox EXPORT ${EXPORT_TARGETS_NAME}Targets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
