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

string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
apply_compile_flags(TESTER_SOURCES "CPP" "C")

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")

	add_library(belle_sip_tester_static STATIC ${TESTER_SOURCES})
	target_include_directories(belle_sip_tester_static PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
	target_link_libraries(belle_sip_tester_static ${BCTOOLBOX_TESTER_LIBRARIES} bellesip)

	set(RUNTIME_COMPONENT_SOURCES
		belle_sip_tester_windows.cpp
		belle_sip_tester_windows.h
	)
	add_library(belle_sip_tester_runtime MODULE ${RUNTIME_COMPONENT_SOURCES})
	target_link_libraries(belle_sip_tester_runtime belle_sip_tester_static)
	set_target_properties(belle_sip_tester_runtime PROPERTIES VS_WINRT_COMPONENT TRUE)
	set_target_properties(belle_sip_tester_runtime PROPERTIES LIBRARY_OUTPUT_NAME "BelledonneCommunications.BelleSip.Tester")
	set_target_properties(belle_sip_tester_runtime PROPERTIES LINK_FLAGS "/WINMDFILE:BelledonneCommunications.BelleSip.Tester.winmd")

	install(TARGETS belle_sip_tester_runtime
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BelledonneCommunications.BelleSip.Tester.winmd" DESTINATION ${CMAKE_INSTALL_LIBDIR})
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/BelledonneCommunications.BelleSip.Tester.pdb
			DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	endif()

else()

	add_executable(belle_sip_tester ${TESTER_SOURCES})
	if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
		set_target_properties(belle_sip_tester PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	target_include_directories(belle_sip_tester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
	target_link_libraries(belle_sip_tester ${BCTOOLBOX_TESTER_LIBRARIES} bellesip)
	add_test(NAME belle_sip_tester COMMAND belle_sip_tester --verbose)


	set(OBJECT_DESCRIBE_SOURCES describe.c)

	apply_compile_flags(OBJECT_DESCRIBE_SOURCES "CPP" "C")
	add_executable(belle_sip_object_describe ${OBJECT_DESCRIBE_SOURCES})
	if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
		set_target_properties(belle_sip_object_describe PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	target_link_libraries(belle_sip_object_describe bellesip)


	set(PARSE_SOURCES parse.c)

	apply_compile_flags(PARSE_SOURCES "CPP" "C")
	add_executable(belle_sip_parse ${PARSE_SOURCES})
	if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
		set_target_properties(belle_sip_parse PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	target_link_libraries(belle_sip_parse bellesip)

endif()
