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

string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")

set(USE_BUNDLE )
if (IOS)
	set(USE_BUNDLE MACOSX_BUNDLE)
endif()

set(MS2_LIBS_FOR_TOOLS mediastreamer2)
if(X11_FOUND)
	list(APPEND MS2_LIBS_FOR_TOOLS X11::X11)
endif()
if(X11_Xext_FOUND)
	list(APPEND MS2_LIBS_FOR_TOOLS X11::Xext)
endif()
if(X11_Xv_FOUND)
	list(APPEND MS2_LIBS_FOR_TOOLS X11::Xv)
endif()

set(simple_executables bench ring mtudiscover tones msaudiocmp)
if(ENABLE_VIDEO)
	list(APPEND simple_executables videodisplay)
	if(X11_FOUND)
		list(APPEND simple_executables test_x11window)
	endif()
endif()

foreach(simple_executable ${simple_executables})
	set(${simple_executable}_SOURCE_FILES ${simple_executable}.c)
	bc_apply_compile_flags(${simple_executable}_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(mediastreamer2-${simple_executable} ${USE_BUNDLE} ${simple_executable}.c)
	target_link_libraries(mediastreamer2-${simple_executable} ${MS2_LIBS_FOR_TOOLS} ${LINK_LIBS})
	set_target_properties(mediastreamer2-${simple_executable} PROPERTIES LINKER_LANGUAGE CXX)
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer2-${simple_executable} PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
endforeach()

set(ECHO_SOURCE_FILES echo.c)
bc_apply_compile_flags(ECHO_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
add_executable(mediastreamer2-echo ${USE_BUNDLE} ${ECHO_SOURCE_FILES}) # Do not name the target "echo" to avoid conflict with the shell echo command
set_target_properties(mediastreamer2-echo PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(mediastreamer2-echo ${MS2_LIBS_FOR_TOOLS} ${LINK_LIBS})
if(LINK_FLAGS_STR)
	set_target_properties(mediastreamer2-echo PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
endif()

if(PCAP_FOUND)
	if(APPLE AND NOT IOS)
		set(SOURCE_FILES_OBJC pcap_player_cocoa.m)
	endif()
	bc_apply_compile_flags(SOURCE_FILES_OBJC STRICT_OPTIONS_CPP STRICT_OPTIONS_OBJC)

	set(PCAP_PLAYBACK_SOURCE_FILES pcap_playback.c common.c)
	bc_apply_compile_flags(PCAP_PLAYBACK_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(mediastreamer2-pcap-playback ${USE_BUNDLE} ${PCAP_PLAYBACK_SOURCE_FILES} ${SOURCE_FILES_OBJC})
	set_target_properties(mediastreamer2-pcap-playback PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(mediastreamer2-pcap-playback ${MS2_LIBS_FOR_TOOLS} ${LINK_LIBS})
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer2-pcap-playback PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
endif()

if (NOT IOS)
	set(MEDIASTREAM_SOURCE_FILES_C mediastream.c common.c)
	set(MEDIASTREAM_SOURCE_FILES_OBJC )
	if(APPLE)
		list(APPEND MEDIASTREAM_SOURCE_FILES_OBJC mediastream_cocoa.m)
	endif()
	bc_apply_compile_flags(MEDIASTREAM_SOURCE_FILES_C STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	bc_apply_compile_flags(MEDIASTREAM_SOURCE_FILES_OBJC STRICT_OPTIONS_CPP STRICT_OPTIONS_OBJC)
	add_executable(mediastreamer2-mediastream ${USE_BUNDLE} ${MEDIASTREAM_SOURCE_FILES_C} ${MEDIASTREAM_SOURCE_FILES_OBJC})
	set_target_properties(mediastreamer2-mediastream PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(mediastreamer2-mediastream ${MS2_LIBS_FOR_TOOLS} ${LINK_LIBS})
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer2-mediastream PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	install(TARGETS mediastreamer2-mediastream
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
endif()

if(ENABLE_VIDEO)
	set(MKVSTREAM_SOURCE_FILES mkvstream.c)
	bc_apply_compile_flags(MKVSTREAM_SOURCE_FILES STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
	add_executable(mediastreamer2-mkvstream ${USE_BUNDLE} ${MKVSTREAM_SOURCE_FILES})
	set_target_properties(mediastreamer2-mkvstream PROPERTIES LINKER_LANGUAGE CXX)
	target_link_libraries(mediastreamer2-mkvstream ${MS2_LIBS_FOR_TOOLS} ${LINK_LIBS})
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer2-mkvstream PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()

	if (NOT IOS)
		install(TARGETS mediastreamer2-mkvstream
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		)
	endif()
endif()
