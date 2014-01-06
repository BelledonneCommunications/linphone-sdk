cmake_minimum_required(VERSION 2.6)
project(MEDIASTREAMER2 C)

option(MS2_ENABLE_OPUS "Build mediastreamer2 with the OPUS codec." ON)

if(NOT ORTP_ROOT_DIR)
	set(ORTP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../oRTP)
endif()
if(NOT ORTP_INCLUDE_DIR)
	set(ORTP_INCLUDE_DIR ${ORTP_ROOT_DIR}/include)
endif()
if(NOT SPEEX_ROOT_DIR)
	set(SPEEX_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../speex)
endif()
if(NOT SPEEX_INCLUDE_DIR)
	set(SPEEX_INCLUDE_DIR ${SPEEX_ROOT_DIR}/include)
endif()

include_directories(
	include/
	src/utils/
	${CMAKE_CURRENT_BINARY_DIR}/src/
)

if(USE_INSTALLED_COMPONENTS)
	include_directories(${CMAKE_INSTALL_PREFIX}/include)
	if(MS2_ENABLE_OPUS)
		include_directories(${CMAKE_INSTALL_PREFIX}/include/opus)
		find_library(LIBOPUS libopus.lib PATHS ${CMAKE_INSTALL_PREFIX}/lib)
	endif(MS2_ENABLE_OPUS)
else()
	include_directories(
		${ORTP_INCLUDE_DIR}
		${SPEEX_INCLUDE_DIR}
	)
	if(WIN32)
		include_directories(${ORTP_ROOT_DIR}/build/vsx/oRTP/oRTP/)
	endif(WIN32)
endif()

add_subdirectory(include)
add_subdirectory(src)

if(INSTALL_COMPONENT_IN_POSTBUILD)
	add_install_target(INSTALL_libmediastreamer2 COMP_libmediastreamer2 "libmediastreamer_base libmediastreamer_voip")
endif()
