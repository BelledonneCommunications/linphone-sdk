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

if(MSVC AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	find_library(LIBGCC NAMES gcc)
	find_library(LIBMINGWEX NAMES mingwex)
endif()

set(LIBS
	${LIBGCC}
	${LIBMINGWEX}
	${MEDIASTREAMER2_LIBRARIES}
	${ORTP_LIBRARIES}
)

set(SOURCE_FILES msamr.c)
if(ENABLE_NARROWBAND)
	list(APPEND SOURCE_FILES amrnb.c)
	list(APPEND LIBS ${OPENCOREAMRNB_LIBRARIES})
endif()
if(ENABLE_WIDEBAND)
	list(APPEND SOURCE_FILES amrwb.c)
	list(APPEND LIBS ${OPENCOREAMRWB_LIBRARIES} ${VOAMRWBENC_LIBRARIES})
endif()

if(ENABLE_STATIC)
	add_library(msamr-static STATIC ${SOURCE_FILES})
	set_target_properties(msamr-static PROPERTIES OUTPUT_NAME msamr)
	target_link_libraries(msamr-static ${LIBS})
	install(TARGETS msamr-static
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/mediastreamer/plugins
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(msamr MODULE ${SOURCE_FILES})
	target_link_libraries(msamr ${LIBS})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/libmsamr.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
		set_target_properties(msamr PROPERTIES PREFIX "lib")
	endif()
	install(TARGETS msamr
		RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/mediastreamer/plugins
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/mediastreamer/plugins
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/mediastreamer/plugins
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
