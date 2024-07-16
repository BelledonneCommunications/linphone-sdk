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

function(require_c99 target)
	if(CMAKE_C_COMPILER_ID STREQUAL "QCC")
		target_compile_options(${target} PRIVATE "-std=gnu99")
	else()
		set_property(TARGET ${target} PROPERTY C_STANDARD 99)
		set_property(TARGET ${target} PROPERTY C_STANDARD_REQUIRED TRUE)
	endif()
endfunction()

set(PUBLIC_INCLUDE_DIRS
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	$<INSTALL_INTERFACE:include/belle-sip>
)
set(LIBS )

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
	list(APPEND LIBS ${ZLIB_TARGET})
endif()
if(DNSSD_FOUND)
	list(APPEND LIBS ${DNSSD_LIBRARIES})
endif()
if(WIN32)
	list(APPEND LIBS ws2_32)
endif()

if(Tunnel_FOUND)
	set(TUNNEL_SOURCE_FILES_C
		transports/tunnel_listeningpoint.c
		transports/tunnel_channel.c
	)
	set(TUNNEL_SOURCE_FILES_CXX
		transports/tunnel_wrapper.cc
	)
endif()

set(BELLE_SIP_SOURCE_FILES_C
	auth_event.c
	auth_helper.c
	belle_sip_loop.c
	belle_sip_mdns_register.c
	belle_sip_object.c
	belle_sip_parameters.c
	belle_sip_utils.c
	belle_sip_resolver.c
	bodyhandler.c
	channel.c
	channel.h
	dialog.c
	dns/dns.c
	dns/dns.h
	http-listener.c
	http-provider.c
	ict.c
	ist.c
	listeningpoint.c
	listeningpoint_internal.h
	md5.c
	md5.h
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
	belle_sdp_impl.cc
	sdp/parser.cc
	belle_sip_headers_impl.cc
	belle_sip_uri_impl.cc
	sip/sip_parser.cc
	bearer_token.cc
	channel_bank.cc
	channel_bank.hh
	generic-uri.cc
	message.cc
	http-message.cc
)
set(GRAMMAR_FILES
	sdp/sdp_grammar
	sip/sip_grammar
)

if(APPLE)
	set(BELLE_SIP_SOURCE_FILES_OBJC
		backgroundtask.mm
		ios_check_version.m
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
else()
	set_property(SOURCE dns/dns.c APPEND_STRING PROPERTY COMPILE_FLAGS " -Wno-override-init -Wno-implicit-fallthrough -Wno-array-bounds -Wno-incompatible-function-pointer-types -Wno-strict-prototypes")
endif()

set(BELLESIP_RESOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/sdp/sdp_grammar"
	"${CMAKE_CURRENT_SOURCE_DIR}/../src/sip/sip_grammar"
)

add_library(belle-sip ${BELLE_SIP_HEADER_FILES} ${BELLE_SIP_SOURCE_FILES_C} ${BELLE_SIP_SOURCE_FILES_CXX} ${BELLE_SIP_SOURCE_FILES_OBJC} ${GRAMMAR_FILES})
require_c99(belle-sip)
set_target_properties(belle-sip PROPERTIES OUTPUT_NAME belle-sip)
set_target_properties(belle-sip PROPERTIES SOVERSION ${BELLESIP_SO_VERSION})
set_target_properties(belle-sip PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(belle-sip PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
target_include_directories(belle-sip INTERFACE ${PUBLIC_INCLUDE_DIRS} PRIVATE ${PRIVATE_INCLUDE_DIRS})
target_link_libraries(belle-sip PUBLIC ${BCToolbox_TARGET} ${Belr_TARGET} PRIVATE ${LIBS})
if(Tunnel_FOUND)
	target_link_libraries(belle-sip PRIVATE ${Tunnel_TARGET})
endif()
if(APPLE_FRAMEWORKS)
	target_link_libraries(belle-sip PRIVATE ${APPLE_FRAMEWORKS})
endif()

if(BUILD_SHARED_LIBS)
	if(APPLE)
		set_target_properties(belle-sip PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER org.linphone.belle-sip
			MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
			RESOURCE "${BELLESIP_RESOURCES}"
			PUBLIC_HEADER "${BELLE_SIP_HEADER_FILES}"
		)
	endif()
	if(MSVC)
		install(FILES $<TARGET_PDB_FILE:belle-sip>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
	endif()
endif()

install(TARGETS belle-sip EXPORT ${PROJECT_NAME}Targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)

install(FILES ${GRAMMAR_FILES}
	DESTINATION "${CMAKE_INSTALL_DATADIR}/belr/grammars"
)
