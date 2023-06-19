############################################################################
# CMakeLists.txt
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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

set(BZRTP_TEST_C_SOURCES
	bzrtpParserTest.c
	bzrtpConfigsTest.c
	bzrtpZidCacheTest.c
	bzrtpTest.c
	testUtils.c
)
set(BZRTP_TEST_CXX_SOURCES
	bzrtpCryptoTest.cc
)

add_definitions(
	-DBCTBX_LOG_DOMAIN="bzrtp"
)

bc_apply_compile_flags(BZRTP_TEST_C_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(BZRTP_TEST_CXX_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

add_executable(bzrtp-tester ${BZRTP_TEST_C_SOURCES} ${BZRTP_TEST_CXX_SOURCES})
set_target_properties(bzrtp-tester PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
set_target_properties(bzrtp-tester PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(bzrtp-tester PRIVATE ${BCToolbox_tester_TARGET} bzrtp)
if(HAVE_SQRT AND NOT WIN32)
	target_link_libraries(bzrtp-tester PRIVATE m)
endif()

install(TARGETS bzrtp-tester
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
install(FILES patternZIDAlice.sqlite DESTINATION "${CMAKE_INSTALL_DATADIR}/bzrtp-tester")
