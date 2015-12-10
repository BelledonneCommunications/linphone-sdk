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

set(TEST_SOURCES
	bzrtpCryptoTest.c
	bzrtpParserTest.c
	bzrtpTest.c
	testUtils.c

	../src/bzrtp.c
	../src/cryptoUtils.c
	../src/packetParser.c
	../src/pgpwords.c
	../src/stateMachine.c
	../src/zidCache.c
)

if (POLARSSL_FOUND)
	set(TEST_SOURCES
		${TEST_SOURCES}
		../src/cryptoPolarssl.c
	)
elseif (MBEDTLS_FOUND)
	set(TEST_SOURCES
		${TEST_SOURCES}
		../src/cryptoMbedtls.c
	)
endif()

apply_compile_flags(TEST_SOURCES "CPP")

add_executable(bzrtpTest ${TEST_SOURCES})
set_target_properties(bzrtpTest PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
target_include_directories(bzrtpTest PUBLIC ${CUNIT_INCLUDE_DIR})
target_link_libraries(bzrtpTest ${CUNIT_LIBRARIES} bzrtp)
if(HAVE_SQRT)
	target_link_libraries(bzrtpTest m)
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
