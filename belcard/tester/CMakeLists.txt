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

file(GLOB SOURCE_FILES_CXX "belcard-*.cpp")

apply_compile_flags(SOURCE_FILES_CXX "CPP" "CXX")

add_executable(belcard-tester ${SOURCE_FILES_CXX})
set_target_properties(belcard-tester PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(belcard-tester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
target_link_libraries(belcard-tester belcard ${BCTOOLBOX_TESTER_LIBRARIES})

if(APPLE)
	set_target_properties(belcard-tester PROPERTIES LINK_FLAGS "-stdlib=libc++")
endif()
