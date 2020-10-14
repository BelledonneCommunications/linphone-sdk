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

function(require_c99 target)
	if(CMAKE_C_COMPILER_ID STREQUAL "QCC")
		target_compile_options(${target} PRIVATE "-std=gnu99")
	else()
		if (CMAKE_VERSION VERSION_LESS "3.1")
			if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
				target_compile_options(${target} PRIVATE "-std=gnu99")
			endif()
		else()
			set_property(TARGET ${target} PROPERTY C_STANDARD 99)
			set_property(TARGET ${target} PROPERTY C_STANDARD_REQUIRED TRUE)
		endif()
	endif()
endfunction()

set(PUBLIC_INCLUDE_DIRS
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	$<INSTALL_INTERFACE:include/belle-sip>
)
set(PRIVATE_INCLUDE_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/antlr3c/include
	${CMAKE_CURRENT_BINARY_DIR}/antlr3c
)
set(LIBS bctoolbox)

if(Threads_FOUND)
	if(CMAKE_USE_PTHREADS_INIT AND NOT CMAKE_SYSTEM_NAME MATCHES "QNX" AND NOT ANDROID)
		list(APPEND LIBS ${CMAKE_THREAD_LIBS_INIT})
	endif()
endif()
if(HAVE_LIBDL)
	list(APPEND LIBS dl)
endif()
if(HAVE_LIBRT)
	list(APPEND LIBS rt)
endif()
if(HAVE_RESINIT)
	list(APPEND LIBS resolv)
endif()
if(ZLIB_FOUND)
	list(APPEND LIBS ${ZLIB_LIBRARIES})
endif()
if(DNSSD_FOUND)
	list(APPEND LIBS ${DNSSD_LIBRARIES})
endif()
if(WIN32)
	list(APPEND LIBS ws2_32)
endif()

list(APPEND LIBS belr)

set(SDP_GENERATED_SOURCE_FILES_C
	grammars/belle_sdpLexer.c
	grammars/belle_sdpLexer.h
	grammars/belle_sdpParser.c
	grammars/belle_sdpParser.h
)
set(SIP_MESSAGE_GENERATED_SOURCE_FILES_C
	grammars/belle_sip_messageLexer.c
	grammars/belle_sip_messageLexer.h
	grammars/belle_sip_messageParser.c
	grammars/belle_sip_messageParser.h
)

if(ENABLE_TUNNEL)
	set(TUNNEL_SOURCE_FILES_C
		transports/tunnel_listeningpoint.c
		transports/tunnel_channel.c
	)
	set(TUNNEL_SOURCE_FILES_CXX
		transports/tunnel_wrapper.cc
	)
	list(APPEND LIBS ${TUNNEL_LIBRARIES})
endif()

add_subdirectory(antlr3c)

set(BELLE_SIP_SOURCE_FILES_C
	auth_event.c
	auth_helper.c
	belle_sip_dict.c
	belle_sip_headers_impl.c
	belle_sip_loop.c
	belle_sip_mdns_register.c
	belle_sip_object.c
	belle_sip_parameters.c
	belle_sip_uri_impl.c
	belle_sip_utils.c
	belle_sip_resolver.c
	bodyhandler.c
	channel.c
	channel.h
	clock_gettime.c
	clock_gettime.h
	dialog.c
	dns/dns.c
	dns/dns.h
	generic-uri.c
	http-listener.c
	http-message.c
	http-provider.c
	ict.c
	ist.c
	listeningpoint.c
	listeningpoint_internal.h
	md5.c
	md5.h
	message.c
	nict.c
	nist.c
	parserutils.h
	port.c
	port.h
	provider.c
	refresher.c
	siplistener.c
	sipstack.c
	transaction.c
	transports/stream_channel.c
	transports/stream_channel.h
	transports/stream_listeningpoint.c
	transports/udp_channel.c
	transports/udp_listeningpoint.c
	transports/tls_listeningpoint.c
	transports/tls_channel.c
	${SDP_GENERATED_SOURCE_FILES_C}
	${SIP_MESSAGE_GENERATED_SOURCE_FILES_C}
	${TUNNEL_SOURCE_FILES_C}
)

add_definitions(
	-DBCTBX_LOG_DOMAIN="belle-sip"
)

if(ANDROID)
	list(APPEND BELLE_SIP_SOURCE_FILES_C wakelock.c)
endif()

set(BELLE_SIP_SOURCE_FILES_CXX
	${TUNNEL_SOURCE_FILES_CXX}
	object++.cc
	cpp_utils.cc
	#cpp_utils.hh
	belle_sdp_impl.cc
	#belle_sip_internal.h
	sdp/parser.cc
	#parser/sdp.hh
)
set(GRAMMAR_FILES
	sdp/sdp_grammar
)

if(APPLE)
	set(BELLE_SIP_SOURCE_FILES_OBJC
		backgroundtask.mm
	)
endif()

bc_apply_compile_flags(BELLE_SIP_SOURCE_FILES_C STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(BELLE_SIP_SOURCE_FILES_OBJC STRICT_OPTIONS_CPP STRICT_OPTIONS_OBJC)
if(BELLE_SIP_SOURCE_FILES_CXX)
	set_source_files_properties(${BELLE_SIP_SOURCE_FILES_CXX} PROPERTIES LANGUAGE CXX)
	bc_apply_compile_flags(BELLE_SIP_SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)
endif()
if(MSVC)
	string(CONCAT DNS_C_COMPILE_FLAGS
		" /wd4267" # Disable "possible loss of data" warnings
		" /wd4804" # Disable "unsafe use of type 'bool' in operation" warnings
		" /wd4244" # Disable "conversion from 'intmax_t' to 'int', possible loss of data" warnings
		" /wd4305" # Disable "'initializing': truncation from 'unsigned int' to 'UCHAR'" warnings
		" /wd4018" # Disable "signed/unsigned mismatch" warnings
	)
	set_property(SOURCE dns/dns.c APPEND_STRING PROPERTY COMPILE_FLAGS ${DNS_C_COMPILE_FLAGS})
	set_property(SOURCE grammars/belle_sdpParser.c APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4267") # Disable "possible loss of data" warnings
	set_property(SOURCE grammars/belle_sip_messageParser.c APPEND_STRING PROPERTY COMPILE_FLAGS " /wd4267") # Disable "possible loss of data" warnings
else()
	set_property(SOURCE dns/dns.c APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-override-init -Wno-implicit-fallthrough -Wno-array-bounds")
	set_property(SOURCE grammars/belle_sip_messageParser.c APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-sign-compare")
endif()

string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
if(ENABLE_STATIC)
	add_library(bellesip-static STATIC ${BELLE_SIP_HEADER_FILES} ${BELLE_SIP_SOURCE_FILES_C} ${BELLE_SIP_SOURCE_FILES_CXX} ${BELLE_SIP_SOURCE_FILES_OBJC} ${ANTLR3C_SOURCE_FILES} ${GRAMMAR_FILES})
	set_target_properties(bellesip-static PROPERTIES OUTPUT_NAME bellesip)
	set_target_properties(bellesip-static PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
	target_include_directories(bellesip-static INTERFACE ${PUBLIC_INCLUDE_DIRS} PRIVATE ${PRIVATE_INCLUDE_DIRS})
	target_link_libraries(bellesip-static PRIVATE ${LIBS})
	if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
		set_target_properties(bellesip-static PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	require_c99(bellesip-static)
	install(TARGETS bellesip-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(bellesip SHARED ${BELLE_SIP_HEADER_FILES} ${BELLE_SIP_SOURCE_FILES_C} ${BELLE_SIP_SOURCE_FILES_CXX} ${BELLE_SIP_SOURCE_FILES_OBJC} ${ANTLR3C_SOURCE_FILES} ${GRAMMAR_FILES})
	set_target_properties(bellesip PROPERTIES SOVERSION ${BELLESIP_SO_VERSION})
	set_target_properties(bellesip PROPERTIES LINKER_LANGUAGE CXX)
	set_target_properties(bellesip PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
	target_include_directories(bellesip INTERFACE ${PUBLIC_INCLUDE_DIRS} PRIVATE ${PRIVATE_INCLUDE_DIRS})
	target_link_libraries(bellesip PRIVATE ${LIBS})
	if(APPLE)
		if(IOS)
			set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		else()
			set(MIN_OS ${CMAKE_OSX_DEPLOYMENT_TARGET})
		endif()
		set_target_properties(bellesip PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.belle-sip
			MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_SOURCE_DIR}/build/osx/Info.plist.in"
			PUBLIC_HEADER "${BELLE_SIP_HEADER_FILES}"
		)
		SET_TARGET_PROPERTIES(bellesip PROPERTIES OUTPUT_NAME belle-sip)
	endif()
	if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
		set_target_properties(bellesip PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	require_c99(bellesip)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES $<TARGET_PDB_FILE:bellesip>
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()

	endif()
	if(APPLE)
		install(TARGETS bellesip EXPORT ${EXPORT_TARGETS_NAME}Targets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			FRAMEWORK DESTINATION Frameworks
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
	else()
		install(TARGETS bellesip EXPORT ${EXPORT_TARGETS_NAME}Targets
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
	endif()
endif()
#to avoid // build to generate code twice in // leading compilation issues. Might probably be done in a better way
if (ENABLE_SHARED AND ENABLE_STATIC)
	add_dependencies(bellesip-static bellesip)
endif()

install(FILES ${GRAMMAR_FILES}
	DESTINATION "${CMAKE_INSTALL_DATADIR}/belr/grammars"
)
