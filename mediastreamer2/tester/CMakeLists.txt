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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

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
)
set(SOURCE_FILES_OBJC )

if(ENABLE_VIDEO)
	list(APPEND SOURCE_FILES_C mediastreamer2_video_stream_tester.c)
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
	target_link_libraries(mediastreamer2_tester_static mediastreamer_voip mediastreamer_base ${BCTOOLBOX_TESTER_LIBRARIES})

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

	add_executable(mediastreamer2_tester ${SOURCE_FILES_C} ${SOURCE_FILES_OBJC})
	target_include_directories(mediastreamer2_tester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
	target_link_libraries(mediastreamer2_tester mediastreamer_voip mediastreamer_base ${BCTOOLBOX_TESTER_LIBRARIES})
	string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer2_tester PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()

endif()
