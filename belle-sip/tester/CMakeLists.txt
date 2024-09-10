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
set(HTTPLIB_COMPILE YES)
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE NO)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE NO)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE NO)
set(HTTPLIB_USE_CERTS_FROM_MACOSX_KEYCHAIN NO)
add_subdirectory(cpp-httplib)

set(SOURCE_FILES_C
	auth_helper_tester.c
	belle_generic_uri_tester.c
	belle_sdp_tester.c
	belle_sip_core_tester.c
	belle_sip_dialog_tester.c
	belle_sip_headers_tester.c
	belle_sip_message_tester.c
	belle_sip_refresher_tester.c
	belle_sip_resolver_tester.c
	belle_sip_tester_runtime.c
	belle_sip_tester.h
	belle_sip_uri_tester.c
	belle_sip_fast_uri_tester.c
	cast_test.c
	register_tester.h
)

set(SOURCE_FILES_CXX
	belle_http_tester.c
	belle_sip_register_tester.c
	belle_sip_tester_utils.cc
	belle_sip_tester_utils.h
	object_tester.cc
)

set_source_files_properties(${SOURCE_FILES_CXX} PROPERTIES LANGUAGE CXX)

set(OTHER_FILES
	tester_hosts
)

add_definitions(
	-DBCTBX_LOG_DOMAIN="belle-sip"
)

set(SOURCE_FILES_OBJC )
if (IOS)
	list(APPEND SOURCE_FILES_OBJC belle_sip_tester_ios.m)
endif()

bc_apply_compile_flags(SOURCE_FILES_C STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)
bc_apply_compile_flags(SOURCE_FILES_OBJC STRICT_OPTIONS_CPP STRICT_OPTIONS_OBJC)


set(USE_BUNDLE )
if (IOS)
	set(USE_BUNDLE MACOSX_BUNDLE)
endif()
add_library(libbelle-sip-tester ${SOURCE_FILES_C} ${SOURCE_FILES_CXX} ${SOURCE_FILES_OBJC})
target_link_libraries(libbelle-sip-tester PRIVATE ${BCToolbox_tester_TARGET} belle-sip)
target_link_libraries(libbelle-sip-tester PUBLIC httplib)
target_include_directories(libbelle-sip-tester INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
$<INSTALL_INTERFACE:include>)

if(NOT IOS)
	install(TARGETS libbelle-sip-tester EXPORT ${PROJECT_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
	install(FILES belle_sip_tester_utils.h
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/belle-sip
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	add_library(belle-sip-tester-static STATIC ${SOURCE_FILES_C} ${SOURCE_FILES_CXX})
	target_link_libraries(belle-sip-tester-static PUBLIC ${BCToolbox_tester_TARGET} belle-sip httplib)

	set(RUNTIME_COMPONENT_SOURCES
		belle_sip_tester_windows.cpp
		belle_sip_tester_windows.h
	)
	add_library(belle-sip-tester-runtime MODULE ${RUNTIME_COMPONENT_SOURCES})
	target_link_libraries(belle-sip-tester-runtime belle-sip-tester-static)
	target_link_options(belle-sip-tester-runtime PRIVATE "/WINMDFILE:BelledonneCommunications.BelleSip.Tester.winmd")
	set_target_properties(belle-sip-tester-runtime PROPERTIES LINKER_LANGUAGE CXX)
	set_target_properties(belle-sip-tester-runtime PROPERTIES VS_WINRT_COMPONENT TRUE)
	set_target_properties(belle-sip-tester-runtime PROPERTIES LIBRARY_OUTPUT_NAME "BelledonneCommunications.BelleSip.Tester")
	set_target_properties(belle-sip-tester-runtime PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
	set_target_properties(belle-sip-tester-runtime PROPERTIES C_STANDARD 99)


	install(TARGETS belle-sip-tester-runtime
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BelledonneCommunications.BelleSip.Tester.winmd" DESTINATION ${CMAKE_INSTALL_LIBDIR})
	install(FILES $<TARGET_PDB_FILE:belle-sip-tester-runtime>
		DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		CONFIGURATIONS Debug RelWithDebInfo
	)

elseif(NOT ANDROID)

	# foo.cpp is an empty file here just to request cmake to use the C++ compiler to link the executable
	# since it depends on libraries written in C++.
	add_executable(belle-sip-tester belle_sip_tester.c belle_sip_tester.h foo.cpp)
	target_link_libraries(belle-sip-tester PRIVATE libbelle-sip-tester belle-sip ${BCToolbox_tester_TARGET})
	if(APPLE_FRAMEWORKS)
		target_link_libraries(belle-sip-tester PRIVATE ${APPLE_FRAMEWORKS})
	endif()
	if(WIN32)
		target_link_libraries(belle-sip-tester PRIVATE "Ws2_32")
	endif()
	add_test(NAME belle-sip-tester COMMAND belle-sip-tester --verbose)
	if(NOT IOS)
		install(TARGETS belle-sip-tester
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	endif()


	set(OBJECT_DESCRIBE_SOURCES describe.c)
	bc_apply_compile_flags(OBJECT_DESCRIBE_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(belle-sip-object-describe ${USE_BUNDLE} ${OBJECT_DESCRIBE_SOURCES})
	set_target_properties(belle-sip-object-describe PROPERTIES LINKER_LANGUAGE CXX)
	if(APPLE_FRAMEWORKS)
		target_link_libraries(belle-sip-object-describe PRIVATE ${APPLE_FRAMEWORKS})
	endif()
	target_link_libraries(belle-sip-object-describe PRIVATE ${BCToolbox_TARGET} belle-sip)

	set(OBJECT_RESOLVE_SOURCES resolve.c)
	bc_apply_compile_flags(OBJECT_DESCRIBE_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(belle-sip-resolve ${USE_BUNDLE} ${OBJECT_RESOLVE_SOURCES})
	set_target_properties(belle-sip-resolve PROPERTIES LINKER_LANGUAGE CXX)
	if(APPLE_FRAMEWORKS)
		target_link_libraries(belle-sip-resolve PRIVATE ${APPLE_FRAMEWORKS})
	endif()
	target_link_libraries(belle-sip-resolve PRIVATE ${BCToolbox_TARGET} belle-sip)

	set(PARSE_SOURCES parse.c)

	bc_apply_compile_flags(PARSE_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(belle-sip-parse ${USE_BUNDLE} ${PARSE_SOURCES})
	set_target_properties(belle-sip-parse PROPERTIES LINKER_LANGUAGE CXX)
	if(APPLE_FRAMEWORKS)
		target_link_libraries(belle-sip-parse PRIVATE ${APPLE_FRAMEWORKS})
	endif()
	target_link_libraries(belle-sip-parse PRIVATE ${BCToolbox_TARGET} belle-sip)

	set(GET_SOURCES get.c)

	bc_apply_compile_flags(GET_SOURCES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(belle-sip-http-get ${USE_BUNDLE} ${GET_SOURCES})
	set_target_properties(belle-sip-http-get PROPERTIES LINKER_LANGUAGE CXX)
	if(APPLE_FRAMEWORKS)
		target_link_libraries(belle-sip-http-get PRIVATE ${APPLE_FRAMEWORKS})
	endif()
	target_link_libraries(belle-sip-http-get PRIVATE ${BCToolbox_TARGET} belle-sip)

	if(ENABLE_SIP_PARSER_BENCHMARK)
		set(BENCH_SOURCES bench.cc)
		add_executable(belle-sip-bench ${USE_BUNDLE} ${BENCH_SOURCES})
		set_target_properties(belle-sip-bench PROPERTIES LINKER_LANGUAGE CXX CXX_STANDARD 17 CXX_STANDARD_REQUIRED TRUE)
		if(APPLE_FRAMEWORKS)
			target_link_libraries(belle-sip-bench PRIVATE ${APPLE_FRAMEWORKS})
		endif()
		target_link_libraries(belle-sip-bench PRIVATE ${BCToolbox_TARGET} belle-sip)
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			target_compile_options(belle-sip-bench PRIVATE "-fbracket-depth=512")
		endif()

	endif()

endif()

install(FILES ${OTHER_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/belle-sip-tester")
