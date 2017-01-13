############################################################################
# CMakeLists.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

if(ENABLE_SHARED)
	set(MS2_LIBS_FOR_TESTER mediastreamer_voip mediastreamer_base)
else()
	set(MS2_LIBS_FOR_TESTER mediastreamer_voip-static mediastreamer_base-static)
	if(XV_FOUND)
		list(APPEND MS2_LIBS_FOR_TESTER Xext)
	endif()
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
	scenarios/poor_jitter_quality_22_filter.pcapng
	scenarios/rtp-120late-1000total.pcapng
	scenarios/rtp-240late-2000total.pcapng
	scenarios/h264_missing_pps_in_second_i_frame.pcap
	scenarios/rtp-534late-24loss-7000total.pcapng
	scenarios/secondvalid.pcapng
	scenarios/opus-poor-quality.pcapng
	scenarios/poor_jitter_quality_22.pcapng
)

set(IOS_RESOURCES_FILES
	sounds
	scenarios
)

set(SOURCE_FILES_C
	mediastreamer2_adaptive_tester.c
	mediastreamer2_audio_stream_tester.c
	mediastreamer2_basic_audio_tester.c
	mediastreamer2_framework_tester.c
	mediastreamer2_neon_tester.c
	mediastreamer2_player_tester.c
	mediastreamer2_sound_card_tester.c
	mediastreamer2_tester.c
	mediastreamer2_tester_private.c
	mediastreamer2_text_stream_tester.c
	mediastreamer2_jitterbuffer_tester.c
)
set(SOURCE_FILES_OBJC )

if(ENABLE_VIDEO)
	list(APPEND SOURCE_FILES_C mediastreamer2_video_stream_tester.c)
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

apply_compile_flags(SOURCE_FILES_C "CPP" "C")
apply_compile_flags(SOURCE_FILES_OBJC "CPP" "OBJC")

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")

	add_library(mediastreamer2_tester_static STATIC ${SOURCE_FILES_C})
	target_include_directories(mediastreamer2_tester_static PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
	target_link_libraries(mediastreamer2_tester_static ${MS2_LIBS_FOR_TESTER} ${BCTOOLBOX_TESTER_LIBRARIES})

	set(RUNTIME_COMPONENT_SOURCES
		mediastreamer2_tester_windows.cpp
		mediastreamer2_tester_windows.h
	)
	add_library(mediastreamer2_tester_runtime MODULE ${RUNTIME_COMPONENT_SOURCES})
	target_include_directories(mediastreamer2_tester_runtime PRIVATE "../../../mswinrtvid")	# HACK!!
	target_link_libraries(mediastreamer2_tester_runtime mediastreamer2_tester_static)
	set_target_properties(mediastreamer2_tester_runtime PROPERTIES VS_WINRT_COMPONENT TRUE)
	set_target_properties(mediastreamer2_tester_runtime PROPERTIES LIBRARY_OUTPUT_NAME "BelledonneCommunications.Mediastreamer2.Tester")
	set_target_properties(mediastreamer2_tester_runtime PROPERTIES LINK_FLAGS "/WINMDFILE:BelledonneCommunications.Mediastreamer2.Tester.winmd")

	install(TARGETS mediastreamer2_tester_runtime
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

else()
	if(IOS)
		set_source_files_properties(${IOS_RESOURCES_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
		add_executable(mediastreamer2_tester MACOSX_BUNDLE ${IOS_RESOURCES_FILES} ${SOURCE_FILES_C} ${SOURCE_FILES_OBJC})
	else()
		add_executable(mediastreamer2_tester ${SOURCE_FILES_C} ${SOURCE_FILES_OBJC})
	endif()
	target_include_directories(mediastreamer2_tester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
	target_link_libraries(mediastreamer2_tester ${MS2_LIBS_FOR_TESTER} ${BCTOOLBOX_TESTER_LIBRARIES})
	string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer2_tester PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	if(NOT IOS)
		install(TARGETS mediastreamer2_tester
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
		install(FILES ${SOUND_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/mediastreamer2_tester/sounds")
		install(FILES ${SCENARIO_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/mediastreamer2_tester/scenarios")
	endif()

endif()

