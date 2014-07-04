cmake_minimum_required(VERSION 2.6)
project(MEDIASTREAMER2 C)

option(MS2_ENABLE_FFMPEG "Build mediastreamer2 with ffmpeg video support (needs MS2_ENABLE_VIDEO)." ON)
option(MS2_ENABLE_GSM "Build mediastreamer2 with the GSM codec." OFF)
option(MS2_ENABLE_OPUS "Build mediastreamer2 with the OPUS codec." ON)
option(MS2_ENABLE_RESAMPLE "Build mediastreamer2 with the resampling capability." ON)
option(MS2_ENABLE_SPEEX "Build mediastreamer2 with the SPEEX codec." ON)
option(MS2_ENABLE_VIDEO "Build mediastreamer2 with video support." ON)
option(MS2_ENABLE_VPX "Build mediastreamer2 with VPX codec (needs MS2_ENABLE_VIDEO)." ON)

include_directories(
	include/
	src/utils/
	${CMAKE_CURRENT_BINARY_DIR}/src/
)

if(MS2_ENABLE_VIDEO)
	include_directories(src/voip)
endif(MS2_ENABLE_VIDEO)

set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${prefix}/bin)
set(libdir ${prefix}/lib)
set(includedir ${prefix}/include)
set(MEDIASTREAMER_VERSION "2.10.0")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mediastreamer.pc.in ${CMAKE_CURRENT_BINARY_DIR}/mediastreamer.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mediastreamer.pc DESTINATION lib/pkgconfig)

include_directories(${CMAKE_INSTALL_PREFIX}/include)
if(MSVC)
	include_directories(${CMAKE_INSTALL_PREFIX}/include/MSVC)
endif()

add_subdirectory(include)
add_subdirectory(src)
