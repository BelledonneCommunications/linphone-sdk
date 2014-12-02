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

if(MSVC)
	find_library(LIBGCC NAMES gcc)
	find_library(LIBMINGWEX NAMES mingwex)
endif()

set(LIBS
	${LIBGCC}
	${LIBMINGWEX}
	${MS2_LIBRARIES}
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

add_library(msamr SHARED ${SOURCE_FILES})
set_target_properties(msamr PROPERTIES VERSION ${PACKAGE_VERSION} SOVERSION 0)
if(MSVC)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/libmsamr.pdb
			DESTINATION bin
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	endif()
	set_target_properties(msamr PROPERTIES PREFIX "lib")
endif()
target_link_libraries(msamr ${LIBS})

install(TARGETS msamr
	RUNTIME DESTINATION lib/mediastreamer/plugins
	LIBRARY DESTINATION lib/mediastreamer/plugins
	ARCHIVE DESTINATION lib/mediastreamer/plugins
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
