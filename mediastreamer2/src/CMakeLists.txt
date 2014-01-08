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
	#audiofilters/genericplc.c
	#audiofilters/gsm.c
	audiofilters/l16.c
	audiofilters/msconf.c
	audiofilters/msfileplayer.c
	audiofilters/msfilerec.c
	audiofilters/msg722.c
	audiofilters/msresample.c
	audiofilters/msspeex.c
	audiofilters/msvolume.c
	audiofilters/speexec.c
	audiofilters/tonedetector.c
	audiofilters/ulaw.c
	audiofilters/waveheader.h
	audiofilters/winsnd3.c
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
	#voip/layouts.c
	voip/mediastream.c
	voip/msvoip.c
	voip/private.h
	voip/qosanalyzer.c
	voip/qualityindicator.c
	voip/ringstream.c
	voip/rfc3984.c
)

if(MS2_ENABLE_OPUS)
set(VOIP_SOURCE_FILES ${VOIP_SOURCE_FILES} audiofilters/msopus.c)
endif(MS2_ENABLE_OPUS)

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
set(VOIP_LIBS ${VOIP_LIBS} libortp libmediastreamer_base libspeex libspeexdsp)
if(MS2_ENABLE_OPUS)
set(VOIP_LIBS ${VOIP_LIBS} ${LIBOPUS})
endif(MS2_ENABLE_OPUS)

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
