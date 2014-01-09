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
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND awk -f ../extract-filters-names.awk ${BASE_SOURCE_FILES}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.txt
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND awk -f ../define-filters.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp1.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND awk -f ../define-ms_base_filter_descs.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp2.h
)
execute_process(
	COMMAND cat ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp1.h ${CMAKE_CURRENT_BINARY_DIR}/basedescs-tmp2.h
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/basedescs.h
)

add_definitions(
	-DMEDIASTREAMER2_EXPORTS
	-DMEDIASTREAMER2_INTERNAL_EXPORTS
	-DHAVE_SPEEXDSP
	-DORTP_INET6
	-D_TRUE_TIME
	-DMS2_INTERNAL
	-DMS2_FILTERS
)

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
)

set(BASE_LIBS ws2_32 ole32 oleaut32 winmm uuid)
endif(WIN32)
set(BASE_LIBS ${LIBS} libortp)

add_library(libmediastreamer_base SHARED ${BASE_SOURCE_FILES} ${BASE_GENERATED_SOURCE_FILES})
set_target_properties(libmediastreamer_base PROPERTIES VERSION 2.9.2 SOVERSION 3)

target_link_libraries(libmediastreamer_base ${BASE_LIBS})

install(TARGETS libmediastreamer_base
		COMPONENT COMP_libmediastreamer2
        DESTINATION ${LIB_INSTALL_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

if(USE_INSTALLED_COMPONENTS)
	add_dependencies(libmediastreamer_base INSTALL_libortp)
endif()



set(VOIP_SOURCE_FILES
	audiofilters/alaw.c
	audiofilters/audiomixer.c
	audiofilters/chanadapt.c
	#audiofilters/devices.c
	#audiofilters/devices.h
	audiofilters/dtmfgen.c
	audiofilters/equalizer.c
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
	audiofilters/flowcontrol.c
	audiofilters/flowcontrol.h
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
			utils/ffmpeg-priv.h
			utils/ffmpeg-priv.c
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
		list(APPEND VOIP_SOURCE_FILES
			videofilters/msdscap-mingw.cc
			videofilters/drawdib-display.c
		)
	endif(WIN32)
endif(MS2_ENABLE_VIDEO)

set(VOIP_GENERATED_SOURCE_FILES
	${CMAKE_CURRENT_BINARY_DIR}/voipdescs.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND awk -f ../extract-filters-names.awk ${VOIP_SOURCE_FILES}
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.txt
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND awk -f ../define-filters.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp1.h
)
execute_process(
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND awk -f ../define-ms_voip_filter_descs.awk
	INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.txt
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp2.h
)
execute_process(
	COMMAND cat ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp1.h ${CMAKE_CURRENT_BINARY_DIR}/voipdescs-tmp2.h
	OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/voipdescs.h
)

if(WIN32)
set(VOIP_LIBS ws2_32 ole32 oleaut32 winmm uuid)
endif(WIN32)
	list(APPEND VOIP_LIBS
		libortp
		libmediastreamer_base
	)
if(MS2_ENABLE_GSM)
	list(APPEND VOIP_LIBS ${LIBGSM})
endif(MS2_ENABLE_GSM)
if(MS2_ENABLE_OPUS)
	list(APPEND VOIP_LIBS ${LIBOPUS})
endif(MS2_ENABLE_OPUS)
if(MS2_ENABLE_SPEEX)
	list(APPEND VOIP_LIBS
		libspeex
		libspeexdsp
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
endif(MS2_ENABLE_VIDEO)

add_library(libmediastreamer_voip SHARED ${VOIP_SOURCE_FILES} ${VOIP_GENERATED_SOURCE_FILES})
set_target_properties(libmediastreamer_voip PROPERTIES VERSION 2.9.2 SOVERSION 3)

target_link_libraries(libmediastreamer_voip ${VOIP_LIBS})

install(TARGETS libmediastreamer_voip
		COMPONENT COMP_libmediastreamer2
        DESTINATION ${LIB_INSTALL_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

if(USE_INSTALLED_COMPONENTS)
	add_dependencies(libmediastreamer_voip
		INSTALL_libortp
		INSTALL_libspeex)
endif()
