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

	${CMAKE_SOURCE_DIR}/src/bzrtp.c
	${CMAKE_SOURCE_DIR}/src/cryptoPolarssl.c
	${CMAKE_SOURCE_DIR}/src/cryptoUtils.c
	${CMAKE_SOURCE_DIR}/src/packetParser.c
	${CMAKE_SOURCE_DIR}/src/stateMachine.c
	${CMAKE_SOURCE_DIR}/src/zidCache.c
)

add_executable(bzrtpTest ${TEST_SOURCES})
set_target_properties(bzrtpTest PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
if("${CMAKE_VERSION}" VERSION_GREATER "2.8.10")
	target_include_directories(bzrtpTest PUBLIC ${CUNIT_INCLUDE_DIR})
else()
	include_directories(${CUNIT_INCLUDE_DIR})
endif()
target_link_libraries(bzrtpTest ${CUNIT_LIBRARIES} bzrtp)
add_test(NAME bzrtpTest COMMAND bzrtpTest)
