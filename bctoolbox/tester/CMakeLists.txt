############################################################################
# CMakeLists.txt
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

if(CUNIT_FOUND)
	set(TESTER_SOURCES
		bctoolbox_tester.c
		bctoolbox_tester.h
		containers.cc
	)
	
	string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
	
	add_executable(bctoolbox_tester ${TESTER_SOURCES})
	if(NOT "${LINK_FLAGS_STR}" STREQUAL "")
		set_target_properties(bctoolbox_tester PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	target_include_directories(bctoolbox_tester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
	target_link_libraries(bctoolbox_tester ${BCTOOLBOX_TESTER_LIBRARIES} bctoolbox bctoolbox-tester)
	set_target_properties(bctoolbox_tester PROPERTIES XCODE_ATTRIBUTE_WARNING_CFLAGS "")
	add_test(NAME bctoolbox_tester COMMAND bctoolbox_tester --verbose)
endif()
