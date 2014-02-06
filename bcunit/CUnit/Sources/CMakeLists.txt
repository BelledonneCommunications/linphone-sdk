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

set(BASIC_SOURCE_FILES Basic/Basic.c)
set(AUTOMATED_SOURCE_FILES Automated/Automated.c)
set(CONSOLE_SOURCE_FILES Console/Console.c)
set(CURSES_SOURCE_FILES Curses/Curses.c)
set(FRAMEWORK_SOURCE_FILES
	Framework/CUError.c
	Framework/MyMem.c
	Framework/TestDB.c
	Framework/TestRun.c
	Framework/Util.c
)

set(SOURCE_FILES ${FRAMEWORK_SOURCE_FILES})
source_group(framework FILES ${FRAMEWORK_SOURCE_FILES})
if(${ENABLE_BASIC})
	list(APPEND SOURCE_FILES ${BASIC_SOURCE_FILES})
	source_group(basic FILES ${BASIC_SOURCE_FILES})
endif(${ENABLE_BASIC})
if(${ENABLE_AUTOMATED})
	list(APPEND SOURCE_FILES ${AUTOMATED_SOURCE_FILES})
	source_group(automated FILES ${AUTOMATED_SOURCE_FILES})
endif(${ENABLE_AUTOMATED})
if(${ENABLE_CONSOLE})
	list(APPEND SOURCE_FILES ${CONSOLE_SOURCE_FILES})
	source_group(console FILES ${CONSOLE_SOURCE_FILES})
endif(${ENABLE_CONSOLE})
if(${ENABLE_CURSES})
	list(APPEND SOURCE_FILES ${CURSES_SOURCE_FILES})
	source_group(curses FILES ${CURSES_SOURCE_FILES})
endif(${ENABLE_CURSES})


if(${ENABLE_STATIC})
	add_library(cunit STATIC ${SOURCE_FILES})
	target_link_libraries(cunit ${CURSES_LIBRARIES})
else(${ENABLE_STATIC})
	add_library(cunit SHARED ${SOURCE_FILES})
	set_target_properties(cunit PROPERTIES SOVERSION 1.0.1 VERSION 1)
	target_link_libraries(cunit ${CURSES_LIBRARIES})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/cunit.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif(MSVC)
endif(${ENABLE_STATIC})
install(TARGETS cunit
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)


if(${ENABLE_TEST})
	add_library(cunit_test STATIC ${FRAMEWORK_SOURCE_FILES})
	target_compile_definitions(cunit_test PUBLIC MEMTRACE CUNIT_BUILD_TESTS CUNIT_DO_NOT_DEFINE_UNLESS_BUILDING_TESTS)
	target_include_directories(cunit_test PUBLIC Test)

	add_subdirectory(Test)
endif(${ENABLE_TEST})
