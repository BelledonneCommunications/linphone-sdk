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

set(simple_executables bench echo ring mtudiscover tones)
if (ENABLE_VIDEO)
	list(APPEND simple_executables videodisplay test_x11window)
endif()
foreach (simple_executable ${simple_executables})
	add_executable(${simple_executable} ${simple_executable}.c)
	target_link_libraries(${simple_executable} mediastreamer_voip mediastreamer_base)
endforeach()

if (PCAP_FOUND)
	add_executable(pcap_playback pcap_playback.c common.c)
	target_link_libraries(pcap_playback mediastreamer_voip mediastreamer_base)
	target_include_directories(pcap_playback PUBLIC ${PCAP_INCLUDE_DIRS})
	target_link_libraries(pcap_playback ${PCAP_LIBRARIES})
endif()

set(MEDIASTREAM_SOURCE_FILES mediastream.c common.c)
if(APPLE)
	list(APPEND MEDIASTREAM_SOURCE_FILES mediastream_cocoa.m)
endif()
add_executable(mediastream ${MEDIASTREAM_SOURCE_FILES})
target_link_libraries(mediastream mediastreamer_voip mediastreamer_base)
string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
	set_target_properties(mediastream PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
endif()

add_executable(mkvstream mkvstream.c)
target_link_libraries(mkvstream mediastreamer_voip mediastreamer_base)

install(TARGETS mediastream mkvstream
	RUNTIME DESTINATION bin
)

