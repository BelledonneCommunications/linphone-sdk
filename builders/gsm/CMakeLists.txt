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

cmake_minimum_required(VERSION 2.6)
project(GSM C)


option(ENABLE_STATIC "Build static library (default is shared library)." OFF)


include_directories(
	inc/
)

set(GSM_SOURCE_FILES
	src/add.c
	src/code.c
	src/debug.c
	src/decode.c
	src/long_term.c
	src/lpc.c
	src/preprocess.c
	src/rpe.c
	src/gsm_destroy.c
	src/gsm_decode.c
	src/gsm_encode.c
	src/gsm_explode.c
	src/gsm_implode.c
	src/gsm_create.c
	src/gsm_print.c
	src/gsm_option.c
	src/short_term.c
	src/table.c
)

if(${ENABLE_STATIC})
	add_library(gsm STATIC ${GSM_SOURCE_FILES})
else(${ENABLE_STATIC})
	add_library(gsm SHARED ${GSM_SOURCE_FILES})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/gsm.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif(MSVC)
endif(${ENABLE_STATIC})

install(TARGETS gsm
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)


file(GLOB HEADER_FILES "inc/gsm.h")

install(FILES ${HEADER_FILES}
	DESTINATION include/gsm
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
