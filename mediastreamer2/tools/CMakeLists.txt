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

set(simple_executables bench ring mtudiscover tones)
if(ENABLE_VIDEO)
	list(APPEND simple_executables videodisplay test_x11window)
endif()
foreach(simple_executable ${simple_executables})
	set(${simple_executable}_SOURCE_FILES ${simple_executable}.c)
	apply_compile_flags(${simple_executable}_SOURCE_FILES "CPP" "C")
	add_executable(${simple_executable} ${simple_executable}.c)
	target_link_libraries(${simple_executable} mediastreamer_voip mediastreamer_base)
endforeach()

set(ECHO_SOURCE_FILES echo.c)
apply_compile_flags(ECHO_SOURCE_FILES "CPP" "C")
add_executable(ms-echo ${ECHO_SOURCE_FILES}) # Do not name the target "echo" to avoid conflict with the shell echo command
target_link_libraries(ms-echo mediastreamer_voip mediastreamer_base)

if(PCAP_FOUND)
	set(PCAP_PLAYBACK_SOURCE_FILES pcap_playback.c common.c)
	apply_compile_flags(PCAP_PLAYBACK_SOURCE_FILES "CPP" "C")
	add_executable(pcap_playback ${PCAP_PLAYBACK_SOURCE_FILES})
	target_link_libraries(pcap_playback mediastreamer_voip mediastreamer_base ${PCAP_LIBRARIES})
	target_include_directories(pcap_playback PUBLIC ${PCAP_INCLUDE_DIRS})
endif()

set(MEDIASTREAM_SOURCE_FILES_C mediastream.c common.c)
set(MEDIASTREAM_SOURCE_FILES_OBJC )
if(APPLE)
	list(APPEND MEDIASTREAM_SOURCE_FILES_OBJC mediastream_cocoa.m)
endif()
apply_compile_flags(MEDIASTREAM_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(MEDIASTREAM_SOURCE_FILES_OBJC "CPP" "OBJC")
add_executable(mediastream ${MEDIASTREAM_SOURCE_FILES_C} ${MEDIASTREAM_SOURCE_FILES_OBJC})
target_link_libraries(mediastream mediastreamer_voip mediastreamer_base)
string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
if(LINK_FLAGS_STR)
	set_target_properties(mediastream PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
endif()

if(ENABLE_VIDEO)
	set(MKVSTREAM_SOURCE_FILES mkvstream.c)
	apply_compile_flags(MKVSTREAM_SOURCE_FILES "CPP" "C")
	add_executable(mkvstream ${MKVSTREAM_SOURCE_FILES})
	target_link_libraries(mkvstream mediastreamer_voip mediastreamer_base)

	install(TARGETS mediastream mkvstream
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
endif()
