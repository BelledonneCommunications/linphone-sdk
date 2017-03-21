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

set(TEST_SOURCES
	bzrtpCryptoTest.c
	bzrtpParserTest.c
	bzrtpConfigsTest.c
	bzrtpTest.c
	testUtils.c

	../src/bzrtp.c
	../src/cryptoUtils.c
	../src/packetParser.c
	../src/pgpwords.c
	../src/stateMachine.c
	../src/zidCache.c
)

bc_apply_compile_flags(TEST_SOURCES STRICT_OPTIONS_CPP)

add_executable(bzrtpTest ${TEST_SOURCES})
set_target_properties(bzrtpTest PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
set_target_properties(bzrtpTest PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(bzrtpTest PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIR})
target_link_libraries(bzrtpTest ${BCTOOLBOX_LIBRARIES})
if(HAVE_SQRT)
	target_link_libraries(bzrtpTest m)
endif()
if(XML2_FOUND)
	target_link_libraries(bzrtpTest ${XML2_LIBRARIES})
endif()

unset(PATTERN_FILES_IN_TEST_DIR CACHE)
find_file(PATTERN_FILES_IN_TEST_DIR
	ZIDAlice.txt ${CMAKE_CURRENT_BINARY_DIR}
)
if (NOT PATTERN_FILES_IN_TEST_DIR)
	file(COPY ZIDAlice.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	file(COPY ZIDBob.txt DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()


add_test(NAME bzrtpTest COMMAND bzrtpTest)
