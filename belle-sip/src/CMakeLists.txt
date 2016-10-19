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

set(INCLUDES ${ANTLR3C_INCLUDE_DIRS})
set(LIBS ${ANTLR3C_LIBRARIES})

list(APPEND INCLUDES ${BCTOOLBOX_CORE_INCLUDE_DIR})
list(APPEND LIBS ${BCTOOLBOX_CORE_LIBRARIES})

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
if(WIN32)
	list(APPEND LIBS ws2_32)
endif()

set(SDP_GENERATED_SOURCE_FILES_C
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sdpLexer.c
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sdpLexer.h
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sdpParser.c
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sdpParser.h
)
add_custom_command(
	OUTPUT ${SDP_GENERATED_SOURCE_FILES_C}
	COMMAND ${ANTLR3_COMMAND} -make -Xmultithreaded -Xconversiontimeout 10000 -fo ${CMAKE_CURRENT_BINARY_DIR}/grammars ${CMAKE_CURRENT_SOURCE_DIR}/grammars/belle_sdp.g
	DEPENDS grammars/belle_sdp.g
)

set(SIP_MESSAGE_GENERATED_SOURCE_FILES_C
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sip_messageLexer.c
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sip_messageLexer.h
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sip_messageParser.c
	${CMAKE_CURRENT_BINARY_DIR}/grammars/belle_sip_messageParser.h
)
add_custom_command(
	OUTPUT ${SIP_MESSAGE_GENERATED_SOURCE_FILES_C}
	COMMAND ${ANTLR3_COMMAND} -make -Xmultithreaded -Xconversiontimeout 10000 -fo ${CMAKE_CURRENT_BINARY_DIR}/grammars ${CMAKE_CURRENT_SOURCE_DIR}/grammars/belle_sip_message.g
	DEPENDS grammars/belle_sip_message.g
)

set_source_files_properties(${SDP_GENERATED_SOURCE_FILES_C} ${SIP_MESSAGE_GENERATED_SOURCE_FILES_C} PROPERTIES GENERATED TRUE)

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

set(BELLE_SIP_SOURCE_FILES_C
	auth_event.c
	auth_helper.c
	belle_sdp_impl.c
	belle_sip_dict.c
	belle_sip_headers_impl.c
	belle_sip_internal.h
	belle_sip_loop.c
	belle_sip_object.c
	belle_sip_parameters.c
	belle_sip_resolver.c
	belle_sip_uri_impl.c
	belle_sip_utils.c
	bodyhandler.c
	channel.c
	channel.h
	clock_gettime.c
	clock_gettime.h
	dialog.c
	dns.c
	dns.h
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

if(ANDROID)
	list(APPEND BELLE_SIP_SOURCE_FILES_C wakelock.c)
endif()
set(BELLE_SIP_SOURCE_FILES_CXX
	${TUNNEL_SOURCE_FILES_CXX}
)
if(APPLE)
	set(BELLE_SIP_SOURCE_FILES_OBJC
		backgroundtask.m
	)
endif()

apply_compile_flags(BELLE_SIP_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(BELLE_SIP_SOURCE_FILES_OBJC "CPP" "OBJC")
if(BELLE_SIP_SOURCE_FILES_CXX)
	set_source_files_properties(${BELLE_SIP_SOURCE_FILES_CXX} PROPERTIES LANGUAGE CXX)
	apply_compile_flags(BELLE_SIP_SOURCE_FILES_CXX "CPP" "CXX")
endif()
if(MSVC)
	get_source_file_property(DNS_C_COMPILE_FLAGS dns.c COMPILE_FLAGS)
	set(DNS_C_COMPILE_FLAGS "${DNS_C_COMPILE_FLAGS} /wd4267") # Disable "possible loss of data" warnings
	set(DNS_C_COMPILE_FLAGS "${DNS_C_COMPILE_FLAGS} /wd4804") # Disable "unsafe use of type 'bool' in operation" warnings
	set(DNS_C_COMPILE_FLAGS "${DNS_C_COMPILE_FLAGS} /wd4244") # Disable "conversion from 'intmax_t' to 'int', possible loss of data" warnings
	set(DNS_C_COMPILE_FLAGS "${DNS_C_COMPILE_FLAGS} /wd4305") # Disable "'initializing': truncation from 'unsigned int' to 'UCHAR'" warnings
	set(DNS_C_COMPILE_FLAGS "${DNS_C_COMPILE_FLAGS} /wd4018") # Disable "signed/unsigned mismatch" warnings
	set_source_files_properties(dns.c PROPERTIES COMPILE_FLAGS "${DNS_C_COMPILE_FLAGS}")
	get_source_file_property(SDP_PARSER_COMPILE_FLAGS grammars/belle_sdpParser.c COMPILE_FLAGS)
	set(SDP_PARSER_COMPILE_FLAGS "${SDP_PARSER_COMPILE_FLAGS} /wd4267") # Disable "possible loss of data" warnings
	set_source_files_properties(grammars/belle_sdpParser.c PROPERTIES COMPILE_FLAGS "${SDP_PARSER_COMPILE_FLAGS}")
	get_source_file_property(SIP_MESSAGE_PARSER_COMPILE_FLAGS grammars/belle_sip_messageParser.c COMPILE_FLAGS)
	set(SIP_MESSAGE_PARSER_COMPILE_FLAGS "${SIP_MESSAGE_PARSER_COMPILE_FLAGS} /wd4267") # Disable "possible loss of data" warnings
	set_source_files_properties(grammars/belle_sip_messageParser.c PROPERTIES COMPILE_FLAGS "${SIP_MESSAGE_PARSER_COMPILE_FLAGS}")
else()
	get_source_file_property(SIP_MESSAGE_PARSER_COMPILE_FLAGS grammars/belle_sip_messageParser.c COMPILE_FLAGS)
	set(SIP_MESSAGE_PARSER_COMPILE_FLAGS "${SIP_MESSAGE_PARSER_COMPILE_FLAGS} -Wno-sign-compare")
	set_source_files_properties(grammars/belle_sip_messageParser.c PROPERTIES COMPILE_FLAGS "${SIP_MESSAGE_PARSER_COMPILE_FLAGS}")
endif()

string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
if(ENABLE_STATIC)
	add_library(bellesip-static STATIC ${BELLE_SIP_HEADER_FILES} ${BELLE_SIP_SOURCE_FILES_C} ${BELLE_SIP_SOURCE_FILES_CXX} ${BELLE_SIP_SOURCE_FILES_OBJC})
	set_target_properties(bellesip-static PROPERTIES OUTPUT_NAME bellesip)
	target_include_directories(bellesip-static PUBLIC ${INCLUDES})
	target_link_libraries(bellesip-static ${LIBS})
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
	add_library(bellesip SHARED ${BELLE_SIP_HEADER_FILES} ${BELLE_SIP_SOURCE_FILES_C} ${BELLE_SIP_SOURCE_FILES_CXX} ${BELLE_SIP_SOURCE_FILES_OBJC})
	set_target_properties(bellesip PROPERTIES SOVERSION 0)
	set_target_properties(bellesip PROPERTIES LINKER_LANGUAGE CXX)
	target_include_directories(bellesip PUBLIC ${INCLUDES})
	target_link_libraries(bellesip PRIVATE ${LIBS})
	if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
		set_target_properties(bellesip PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	require_c99(bellesip)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bellesip.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	install(TARGETS bellesip EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()



