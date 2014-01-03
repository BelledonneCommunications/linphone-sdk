cmake_minimum_required(VERSION 2.6)
project(BELLESIP C)

if(NOT ANTLR3_ROOT_DIR)
	set(ANTLR3_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../antlr3)
endif()
if(NOT ANTLR3C_INCLUDE_DIR)
	set(ANTLR3C_INCLUDE_DIR ${ANTLR3_ROOT_DIR}/runtime/C/include)
endif()
if(NOT POLARSSL_INCLUDE_DIR)
	set(POLARSSL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../polarssl/include)
endif()
if(NOT ORTP_ROOT_DIR)
	set(ORTP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../linphone/oRTP)
endif()

include_directories(
	include/
	${CMAKE_CURRENT_BINARY_DIR}/src/
	src/
)

if(USE_INSTALLED_COMPONENTS)
	include_directories(${CMAKE_INSTALL_PREFIX}/include)
else()
	include_directories(
		${ANTLR3C_INCLUDE_DIR}
		${POLARSSL_INCLUDE_DIR}
	)
	if(WIN32)
		include_directories(${ORTP_ROOT_DIR}/build/vsx/oRTP/oRTP/)
	endif(WIN32)
endif()

add_subdirectory(include)
add_subdirectory(src)

if(INSTALL_COMPONENT_IN_POSTBUILD)
	add_install_target(INSTALL_libbellesip COMP_libbellesip libbellesip)
endif()
