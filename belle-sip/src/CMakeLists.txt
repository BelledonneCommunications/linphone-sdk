find_library(LIBANTLR3C NAMES antlr3c)
find_library(LIBPOLARSSL NAMES polarssl)

set(SDP_GENERATED_SOURCE_FILES
	${CMAKE_CURRENT_BINARY_DIR}/belle_sdpLexer.c
	${CMAKE_CURRENT_BINARY_DIR}/belle_sdpLexer.h
	${CMAKE_CURRENT_BINARY_DIR}/belle_sdpParser.c
	${CMAKE_CURRENT_BINARY_DIR}/belle_sdpParser.h
)
add_custom_command(
	OUTPUT ${SDP_GENERATED_SOURCE_FILES}
	COMMAND java -jar ${ANTLR3_ROOT_DIR}/antlr-3.4-complete.jar -make -fo ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/belle_sdp.g
	DEPENDS belle_sdp.g
)

set(SIP_MESSAGE_GENERATED_SOURCE_FILES
	${CMAKE_CURRENT_BINARY_DIR}/belle_sip_messageLexer.c
	${CMAKE_CURRENT_BINARY_DIR}/belle_sip_messageLexer.h
	${CMAKE_CURRENT_BINARY_DIR}/belle_sip_messageParser.c
	${CMAKE_CURRENT_BINARY_DIR}/belle_sip_messageParser.h
)
add_custom_command(
	OUTPUT ${SIP_MESSAGE_GENERATED_SOURCE_FILES}
	COMMAND java -jar ${ANTLR3_ROOT_DIR}/antlr-3.4-complete.jar -make -fo ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/belle_sip_message.g
	DEPENDS belle_sip_message.g
)

set(SOURCE_FILES
	auth_event.c
	auth_helper.c
	belle_sdp_impl.c
	belle_sip_headers_impl.c
	belle_sip_loop.c
	belle_sip_object.c
	belle_sip_parameters.c
	belle_sip_resolver.c
	belle_sip_uri_impl.c
	belle_sip_utils.c
	channel.c
	dialog.c
	dns.c
	ict.c
	ist.c
	listeningpoint.c
	md5.c
	message.c
	nict.c
	nist.c
	port.c
	provider.c
	refresher.c
	siplistener.c
	sipstack.c
	transaction.c
	transports/stream_channel.c
	transports/stream_listeningpoint.c
	transports/tls_channel_polarssl.c
	transports/tls_listeningpoint_polarssl.c
	transports/udp_channel.c
	transports/udp_listeningpoint.c
	${SDP_GENERATED_SOURCE_FILES}
	${SIP_MESSAGE_GENERATED_SOURCE_FILES}
)

add_definitions(
	-DBELLESIP_EXPORTS
	-DBELLESIP_INTERNAL_EXPORTS
	-DHAVE_POLARSSL
	-DHAVE_ANTLR_STRING_STREAM_NEW
	-DPACKAGE_VERSION="1.2.4"
)

if(WIN32)
set(LIBS ws2_32)
endif(WIN32)
set(LIBS ${LIBS} ${LIBANTLR3C} ${LIBPOLARSSL})

add_library(bellesip SHARED ${SOURCE_FILES})
set_target_properties(bellesip PROPERTIES VERSION 1.2.4 SOVERSION 1)

target_link_libraries(bellesip ${LIBS})

install(TARGETS bellesip
	DESTINATION ${LIB_INSTALL_DIR}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
if(WIN32)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/bellesip.pdb
	DESTINATION ${LIB_INSTALL_DIR}
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
endif()
endif(WIN32)
