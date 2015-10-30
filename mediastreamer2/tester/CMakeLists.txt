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

set(SOURCE_FILES
	common/bc_tester_utils.c
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

add_definitions(-DBC_CONFIG_FILE="mediastreamer-config.h")

if(ENABLE_VIDEO)
	list(APPEND SOURCE_FILES mediastreamer2_video_stream_tester.c)
endif()
if(APPLE)
	list(APPEND SOURCE_FILES mediastreamer2_tester_cocoa.m)
endif()

add_executable(mediastreamer2_tester ${SOURCE_FILES})
target_include_directories(mediastreamer2_tester PUBLIC ${CUNIT_INCLUDE_DIRS} PRIVATE common)
target_link_libraries(mediastreamer2_tester mediastreamer_voip mediastreamer_base ${CUNIT_LIBRARIES})
target_compile_options(mediastreamer2_tester PRIVATE ${STRICT_OPTIONS_CPP} ${STRICT_OPTIONS_C})
string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
	set_target_properties(mediastreamer2_tester PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
endif()
