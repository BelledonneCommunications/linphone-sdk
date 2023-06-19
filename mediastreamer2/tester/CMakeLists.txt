############################################################################
# Copyright (c) 2010-2023 Belledonne Communications SARL.
#
# This file is part of mediastreamer2 
# (see https://gitlab.linphone.org/BC/public/mediastreamer2).
#
############################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################

set(MS2_LIBS_FOR_TESTER mediastreamer2)
if(X11_FOUND)
	list(APPEND MS2_LIBS_FOR_TESTER X11::X11)
endif()
if(X11_Xext_FOUND)
	list(APPEND MS2_LIBS_FOR_TESTER X11::Xext)
endif()
if(X11_Xv_FOUND)
	list(APPEND MS2_LIBS_FOR_TESTER X11::Xv)
endif()

if(WIN32)
    # Required for htons()
    list(APPEND MS2_LIBS_FOR_TESTER Ws2_32)
endif()

if(SpeexDSP_FOUND)
	# Required for neon resampler tester
	list(APPEND MS2_LIBS_FOR_TESTER ${SpeexDSP_TARGET})
endif()

set(SOUND_FILES
	sounds/arpeggio_8000_mono.wav
	sounds/bird_44100_stereo.wav
	sounds/chimes_48000_stereo.wav
	sounds/hello8000.wav
	sounds/hello8000-1s.wav
	sounds/hello16000.wav
	sounds/hello16000-1s.wav
	sounds/hello_opus.mka
	sounds/hello_pcmu.mka
	sounds/laserrocket_16000_mono.wav
	sounds/nylon_48000_mono.wav
	sounds/owl_44100_mono.wav
	sounds/piano_8000_stereo.wav
	sounds/punch_16000_stereo.wav
	sounds/sintel_trailer_opus_h264.mkv
	sounds/sintel_trailer_opus_vp8.mkv
	sounds/sintel_trailer_pcmu_h264.mkv
	sounds/test_silence_voice_48000.wav
	sounds/test_silence_voice_44100.wav
	sounds/test_silence_voice_32000.wav
	sounds/test_silence_voice_16000.wav
	sounds/test_silence_voice_8000.wav
)

set(SCENARIO_FILES
	scenarios/congestion/video-160-120-0f20c60.pcapng
	scenarios/congestion/audio-80-75-0c60.pcapng
	scenarios/congestion/audio-80-75-0f35c95f120.pcapng
	scenarios/congestion/video-160-0-0a5f30.pcapng
	scenarios/congestion/video-160-90-0a5c30.pcapng
	scenarios/pcmu_8k_no_jitter.pcap
	scenarios/rtp-60late-500total.pcapng
	scenarios/firstvalid.pcapng
	scenarios/opus-edge-congestion20_60_40.pcapng
	scenarios/h264_one_nalu_per_frame.pcap
	scenarios/h264_one_nalu_per_frame_with_corrupted_idr.pcap
	scenarios/poor_jitter_quality_22_filter.pcapng
	scenarios/rtp-120late-1000total.pcapng
	scenarios/rtp-240late-2000total.pcapng
	scenarios/h264_missing_pps_in_second_i_frame.pcap
	scenarios/rtp-534late-24loss-7000total.pcapng
	scenarios/secondvalid.pcapng
	scenarios/opus-poor-quality.pcapng
	scenarios/poor_jitter_quality_22.pcapng
)

set(IMAGE_FILES
	images/qrcodesite.jpg
	images/qrcodesite_screen.jpg
	images/qrcodesite_inclined.jpg
	images/qrcodesite_captured.jpg
)

set(RAW_FILES
	raw/h265-iframe
	raw/h265-parameter-sets-frame
)

set(IOS_RESOURCES_FILES
	sounds
	scenarios
	images
)

set(SOURCE_FILES_C
	mediastreamer2_adaptive_tester.c
	mediastreamer2_audio_stream_tester.c
	mediastreamer2_basic_audio_tester.c
	mediastreamer2_framework_tester.c
	mediastreamer2_player_tester.c
	mediastreamer2_recorder_tester.c
	mediastreamer2_sound_card_tester.c
	mediastreamer2_tester.c
	mediastreamer2_tester_private.c
	mediastreamer2_text_stream_tester.c
	mediastreamer2_jitterbuffer_tester.c
)

if(SpeexDSP_FOUND)
	list(APPEND SOURCE_FILES_C mediastreamer2_neon_tester.c)
endif()

set(SOURCE_FILES_CXX
	mediastreamer2_double_encryption_tester.cpp
	mediastreamer2_tester_utils.cpp
)
set(SOURCE_FILES_OBJC )

add_definitions(
	-DBCTBX_LOG_DOMAIN="mediastreamer"
)

# Required for the "EGL OpenGL contexts" test in mediastreamer2_player_tester.c
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../include/OpenGL")

if(ENABLE_VIDEO)
	list(APPEND SOURCE_FILES_C mediastreamer2_video_stream_tester.c)
	list(APPEND SOURCE_FILES_C filters/framemarking_tester.c)
	list(APPEND SOURCE_FILES_CXX mediastreamer2_h26x_tools_tester.cpp)
	if(ENABLE_QRCODE)
		list(APPEND SOURCE_FILES_C mediastreamer2_qrcode_tester.c)
	endif()
	if(ENABLE_PCAP)
		list(APPEND SOURCE_FILES_C mediastreamer2_codec_impl_testers.c)
	endif()
endif()
if(APPLE)
	if (IOS)
		list(APPEND SOURCE_FILES_OBJC mediastreamer2_tester_ios.m)
	else()
		list(APPEND SOURCE_FILES_OBJC mediastreamer2_tester_cocoa.m)
	endif()
endif()

bc_apply_compile_flags(SOURCE_FILES_C STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)
bc_apply_compile_flags(SOURCE_FILES_OBJC STRICT_OPTIONS_CPP STRICT_OPTIONS_OBJC)

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")

	add_library(mediastreamer2-tester-static STATIC ${SOURCE_FILES_C} ${SOURCE_FILES_CXX})
	target_link_libraries(mediastreamer2-tester-static ${MS2_LIBS_FOR_TESTER} ${BCToolbox_tester_TARGET} ${Ortp_TARGET})

	set(RUNTIME_COMPONENT_SOURCES
		mediastreamer2_tester_windows.cpp
		mediastreamer2_tester_windows.h
	)
	add_library(mediastreamer2-tester-runtime MODULE ${RUNTIME_COMPONENT_SOURCES})
	target_include_directories(mediastreamer2-tester-runtime PRIVATE "../../mswinrtvid")	# HACK!!
	target_link_libraries(mediastreamer2-tester-runtime mediastreamer2-tester-static)
	set_target_properties(mediastreamer2-tester-runtime PROPERTIES VS_WINRT_COMPONENT TRUE)
	set_target_properties(mediastreamer2-tester-runtime PROPERTIES LIBRARY_OUTPUT_NAME "BelledonneCommunications.Mediastreamer2.Tester")
	set_target_properties(mediastreamer2-tester-runtime PROPERTIES LINK_FLAGS "/WINMDFILE:BelledonneCommunications.Mediastreamer2.Tester.winmd")

	install(TARGETS mediastreamer2-tester-runtime
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/BelledonneCommunications.Mediastreamer2.Tester.winmd" DESTINATION lib)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/BelledonneCommunications.Mediastreamer2.Tester.pdb
			DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	endif()

elseif(NOT ANDROID)
	if(IOS)
		set_source_files_properties(${IOS_RESOURCES_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
		add_executable(mediastreamer2-tester MACOSX_BUNDLE ${IOS_RESOURCES_FILES} ${SOURCE_FILES_C} ${SOURCE_FILES_CXX} ${SOURCE_FILES_OBJC})
		set_target_properties(mediastreamer2-tester PROPERTIES
					MACOSX_FRAMEWORK_INFO_PLIST Info.plist.in
					MACOSX_BUNDLE_BUNDLE_NAME org.linphone.ms2.mediastreamer2-tester
					MACOSX_BUNDLE_BUNDLE_VERSION 1.0
					MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
		
	else()
		add_executable(mediastreamer2-tester ${SOURCE_FILES_C} ${SOURCE_FILES_CXX} ${SOURCE_FILES_OBJC})
	endif()
	set_target_properties(mediastreamer2-tester PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(mediastreamer2-tester ${MS2_LIBS_FOR_TESTER} ${BCToolbox_tester_TARGET} ${Ortp_TARGET})
	string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer2-tester PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	if(NOT IOS)
		install(TARGETS mediastreamer2-tester
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
		install(FILES ${SOUND_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/mediastreamer2-tester/sounds")
		install(FILES ${SCENARIO_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/mediastreamer2-tester/scenarios")
		install(FILES ${RAW_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/mediastreamer2-tester/raw")
		if(ENABLE_VIDEO AND ENABLE_QRCODE)
			install(FILES ${IMAGE_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/mediastreamer2-tester/images")
		endif()
	endif()

endif()
