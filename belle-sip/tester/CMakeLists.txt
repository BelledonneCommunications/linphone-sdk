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

set(TESTER_SOURCES
	auth_helper_tester.c
	belle_generic_uri_tester.c
	belle_http_tester.c
	belle_sdp_tester.c
	belle_sip_core_tester.c
	belle_sip_dialog_tester.c
	belle_sip_headers_tester.c
	belle_sip_message_tester.c
	belle_sip_refresher_tester.c
	belle_sip_register_tester.c
	belle_sip_resolver_tester.c
	belle_sip_tester.c
	belle_sip_tester.h
	belle_sip_uri_tester.c
	cast_test.c
	register_tester.h
)

add_executable(belle_sip_tester ${TESTER_SOURCES})
set_target_properties(belle_sip_tester PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
if("${CMAKE_VERSION}" VERSION_GREATER "2.8.10")
	target_include_directories(belle_sip_tester PUBLIC ${CUNIT_INCLUDE_DIR})
else()
	include_directories(${CUNIT_INCLUDE_DIR})
endif()
target_link_libraries(belle_sip_tester ${CUNIT_LIBRARIES} bellesip)
add_test(NAME belle_sip_tester COMMAND belle_sip_tester --verbose)


set(OBJECT_DESCRIBE_SOURCES describe.c)

add_executable(belle_sip_object_describe ${OBJECT_DESCRIBE_SOURCES})
set_target_properties(belle_sip_object_describe PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
target_link_libraries(belle_sip_object_describe bellesip)


set(PARSE_SOURCES parse.c)

add_executable(belle_sip_parse ${PARSE_SOURCES})
set_target_properties(belle_sip_parse PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
target_link_libraries(belle_sip_parse bellesip)
