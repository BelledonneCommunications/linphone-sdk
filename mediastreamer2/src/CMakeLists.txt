find_library(LIBORTP NAMES ortp)
if(MS2_ENABLE_GSM)
	find_library(LIBGSM NAMES gsm)
endif(MS2_ENABLE_GSM)
if(MS2_ENABLE_OPUS)
	include_directories(${CMAKE_INSTALL_PREFIX}/include/opus)
	find_library(LIBOPUS NAMES opus)
endif(MS2_ENABLE_OPUS)
if(MS2_ENABLE_SPEEX)
	find_library(LIBSPEEX NAMES speex)
	find_library(LIBSPEEXDSP NAMES speexdsp)
endif(MS2_ENABLE_SPEEX)
if(MS2_ENABLE_VIDEO)
	if(MS2_ENABLE_FFMPEG)
		find_library(LIBAVCODEC NAMES avcodec)
		find_library(LIBAVUTIL NAMES avutil)
		find_library(LIBSWSCALE NAMES swscale)
	endif(MS2_ENABLE_FFMPEG)
	if(MS2_ENABLE_VPX)
		find_library(LIBVPX NAMES vpx)
	endif(MS2_ENABLE_VPX)
endif(MS2_ENABLE_VIDEO)


find_program(GIT_PROGRAM git)
find_program(AWK_PROGRAM awk)
if(NOT AWK_PROGRAM)
	message(FATAL_ERROR "You need the awk program to build mediastreamer2.")
endif(NOT AWK_PROGRAM)

set(BASE_SOURCE_FILES
	base/eventqueue.c
	base/mscommon.c
	base/msfilter.c
	base/msqueue.c
	base/mssndcard.c
	base/msticker.c
	base/mswebcam.c
	base/mtu.c
	otherfilters/itc.c
	otherfilters/join.c
	otherfilters/tee.c
	otherfilters/void.c
)

set(BASE_GENERATED_SOURCE_FILES
	${CMAKE_CURRENT_BINARY_DIR}/basedescs.h
	${CMAKE_CURRENT_BINARY_DIR}/gitversion.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${AWK_PROGRAM} -f ../extract-filters-names.awk ${BASE_SOURCE_FILES}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.txt
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${AWK_PROGRAM} -f ../define-filters.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp1.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${AWK_PROGRAM} -f ../define-ms_base_filter_descs.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp2.h
)
file(READ ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp1.h BASEDESCS1)
file(READ ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp2.h BASEDESCS2)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.h "${BASEDESCS1}${BASEDESCS2}")

if(GIT_PROGRAM)
	execute_process(
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMAND ${GIT_PROGRAM} describe --always
		OUTPUT_VARIABLE GIT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E echo "#define GIT_VERSION \"${GIT_VERSION}\""
		OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/gitversion.h
	)
else(GIT_PROGRAM)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -E echo "#define GIT_VERSION \"unknown\""
		OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/gitversion.h
	)
endif(GIT_PROGRAM)

add_definitions(
	-DMEDIASTREAMER2_EXPORTS
	-DMEDIASTREAMER2_INTERNAL_EXPORTS
	-DORTP_INET6
	-D_TRUE_TIME
	-DMS2_INTERNAL
	-DMS2_FILTERS
)
if(MS2_ENABLE_SPEEX)
add_definitions(-DHAVE_SPEEXDSP)
endif(MS2_ENABLE_SPEEX)

if(MS2_ENABLE_VIDEO)
	add_definitions(-DVIDEO_ENABLED)
	if(MS2_ENABLE_FFMPEG)
		add_definitions(
			-DHAVE_LIBAVCODEC_AVCODEC_H
			-DHAVE_LIBSWSCALE_SWSCALE_H
		)
	else(MS2_ENABLE_FFMPEG)
		add_definitions(-DNO_FFMPEG)
	endif(MS2_ENABLE_FFMPEG)
endif(MS2_ENABLE_VIDEO)

if(WIN32)
	add_definitions(
		-DWINDOW_NATIVE
		/FIgitversion.h
	)
	set(BASE_LIBS ws2_32 ole32 oleaut32 winmm uuid)
endif(WIN32)
set(BASE_LIBS ${BASE_LIBS} ${LIBORTP})

add_library(mediastreamer_base SHARED ${BASE_SOURCE_FILES} ${BASE_GENERATED_SOURCE_FILES})
set_target_properties(mediastreamer_base PROPERTIES VERSION 2.10.0 SOVERSION 3)

target_link_libraries(mediastreamer_base ${BASE_LIBS})

install(TARGETS mediastreamer_base
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
if(WIN32)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/mediastreamer_base.pdb
	DESTINATION bin
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
endif()
endif(WIN32)



set(VOIP_SOURCE_FILES
	audiofilters/alaw.c
	audiofilters/audiomixer.c
	audiofilters/chanadapt.c
	audiofilters/devices.c
	audiofilters/devices.h
	audiofilters/dtmfgen.c
	audiofilters/equalizer.c
	audiofilters/flowcontrol.c
	audiofilters/flowcontrol.h
	audiofilters/genericplc.c
	audiofilters/l16.c
	audiofilters/msconf.c
	audiofilters/msfileplayer.c
	audiofilters/msfilerec.c
	audiofilters/msg722.c
	audiofilters/msvolume.c
	audiofilters/tonedetector.c
	audiofilters/ulaw.c
	audiofilters/waveheader.h
	otherfilters/msrtp.c
	utils/_kiss_fft_guts.h
	utils/dsptools.c
	utils/g711common.h
	utils/g722.h
	utils/g722_decode.c
	utils/g722_encode.c
	utils/kiss_fft.c
	utils/kiss_fft.h
	utils/kiss_fftr.c
	utils/kiss_fftr.h
	voip/audioconference.c
	voip/audiostream.c
	voip/bitratecontrol.c
	voip/bitratedriver.c
	voip/ice.c
	voip/mediastream.c
	voip/msvoip.c
	voip/private.h
	voip/qosanalyzer.c
	voip/qualityindicator.c
	voip/ringstream.c
	voip/rfc3984.c
)

if(WIN32)
	list(APPEND VOIP_SOURCE_FILES audiofilters/winsnd3.c)
endif(WIN32)

if(APPLE)
	add_definitions(-D__MACSND_ENABLED__)
	list(APPEND VOIP_SOURCE_FILES audiofilters/macsnd.c)
endif(APPLE)

if(MS2_ENABLE_GSM)
	list(APPEND VOIP_SOURCE_FILES audiofilters/gsm.c)
endif(MS2_ENABLE_GSM)

if(MS2_ENABLE_OPUS)
	list(APPEND VOIP_SOURCE_FILES audiofilters/msopus.c)
endif(MS2_ENABLE_OPUS)

if(MS2_ENABLE_RESAMPLE)
	list(APPEND VOIP_SOURCE_FILES audiofilters/msresample.c)
endif(MS2_ENABLE_RESAMPLE)

if(MS2_ENABLE_SPEEX)
	list(APPEND VOIP_SOURCE_FILES
		audiofilters/msspeex.c
		audiofilters/speexec.c
	)
endif(MS2_ENABLE_SPEEX)

if(MS2_ENABLE_VIDEO)
	list(APPEND VOIP_SOURCE_FILES
		videofilters/extdisplay.c
		videofilters/mire.c
		videofilters/pixconv.c
		videofilters/sizeconv.c
		voip/layouts.c
		voip/layouts.h
		voip/msvideo.c
		voip/msvideo_neon.c
		voip/msvideo_neon.h
		voip/rfc2429.h
		voip/rfc3984.c
		voip/videostream.c
	)
	if(MS2_ENABLE_FFMPEG)
		list(APPEND VOIP_SOURCE_FILES
			utils/ffmpeg-priv.c
			utils/ffmpeg-priv.h
			utils/swscale.h
			videofilters/h264dec.c
			videofilters/jpegwriter.c
			videofilters/nowebcam.c
			videofilters/videodec.c
			videofilters/videoenc.c
			voip/nowebcam.h
		)
	endif(MS2_ENABLE_FFMPEG)
	if(MS2_ENABLE_VPX)
		list(APPEND VOIP_SOURCE_FILES videofilters/vp8.c)
	endif(MS2_ENABLE_VPX)
	if(WIN32)
		add_definitions(-DHAVE_DIRECTSHOW)
		list(APPEND VOIP_SOURCE_FILES
			videofilters/msdscap-mingw.cc
			videofilters/drawdib-display.c
		)
		set_source_files_properties(videofilters/msdscap-mingw.cc PROPERTIES LANGUAGE CXX)
	endif(WIN32)
	if(APPLE)
		list(APPEND VOIP_SOURCE_FILES
			utils/opengles_display.c
			utils/opengles_display.h
			utils/shaders.c
			utils/shaders.h
			videofilters/msosxdisplay.m
			videofilters/qtcapture.m
		)
	endif(APPLE)
endif(MS2_ENABLE_VIDEO)

set(VOIP_GENERATED_SOURCE_FILES
	${CMAKE_CURRENT_BINARY_DIR}/voipdescs.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${AWK_PROGRAM} -f ../extract-filters-names.awk ${VOIP_SOURCE_FILES}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.txt
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${AWK_PROGRAM} -f ../define-filters.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp1.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${AWK_PROGRAM} -f ../define-ms_voip_filter_descs.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp2.h
)
file(READ ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp1.h VOIPDESCS1)
file(READ ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp2.h VOIPDESCS2)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.h "${VOIPDESCS1}${VOIPDESCS2}")

if(APPLE)
list(APPEND VOIP_GENERATED_SOURCE_FILES
	${CMAKE_CURRENT_BINARY_DIR}/yuv2rgb.fs.h
	${CMAKE_CURRENT_BINARY_DIR}/yuv2rgb.vs.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND xxd -i yuv2rgb.fs | sed s/}\;/,0x00}\;/
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/yuv2rgb.fs.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND xxd -i yuv2rgb.vs | sed s/}\;/,0x00}\;/
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/yuv2rgb.vs.h
)
endif(APPLE)

if(WIN32)
	set(VOIP_LIBS ws2_32 ole32 oleaut32 winmm uuid)
	if(MS2_ENABLE_VIDEO)
		list(APPEND VOIP_LIBS gdi32 user32)
	endif(MS2_ENABLE_VIDEO)
endif(WIN32)
list(APPEND VOIP_LIBS
	${LIBORTP}
	mediastreamer_base
)
if(MS2_ENABLE_GSM)
	list(APPEND VOIP_LIBS ${LIBGSM})
endif(MS2_ENABLE_GSM)
if(MS2_ENABLE_OPUS)
	list(APPEND VOIP_LIBS ${LIBOPUS})
endif(MS2_ENABLE_OPUS)
if(MS2_ENABLE_SPEEX)
	list(APPEND VOIP_LIBS
		${LIBSPEEX}
		${LIBSPEEXDSP}
	)
endif(MS2_ENABLE_SPEEX)
if(MS2_ENABLE_VIDEO)
	if(MS2_ENABLE_FFMPEG)
		list(APPEND VOIP_LIBS
			${LIBAVCODEC}
			${LIBAVUTIL}
			${LIBSWSCALE}
		)
		if(WIN32)
			list(APPEND VOIP_LIBS vfw32)
		endif(WIN32)
	endif(MS2_ENABLE_FFMPEG)
	if(MS2_ENABLE_VPX)
		list(APPEND VOIP_LIBS ${LIBVPX})
	endif(MS2_ENABLE_VPX)
	if(APPLE)
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -framework Cocoa -framework OpenGL -framework QuartzCore")
	endif(APPLE)
endif(MS2_ENABLE_VIDEO)

add_library(mediastreamer_voip SHARED ${VOIP_SOURCE_FILES} ${VOIP_GENERATED_SOURCE_FILES})
set_target_properties(mediastreamer_voip PROPERTIES VERSION 2.10.0 SOVERSION 3 LINKER_LANGUAGE CXX)

target_link_libraries(mediastreamer_voip ${VOIP_LIBS})

install(TARGETS mediastreamer_voip
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
if(WIN32)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/mediastreamer_voip.pdb
	DESTINATION bin
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
endif()
endif(WIN32)
