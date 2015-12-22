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
	common/bc_tester_utils.c
)

set(SOURCE_FILES_CXX
	belcard-addressing-tester.cpp
	belcard-calendar-tester.cpp
	belcard-communication-tester.cpp
	belcard-explanatory-tester.cpp
	belcard-general-tester.cpp
	belcard-geographical-tester.cpp
	belcard-identification-tester.cpp
	belcard-organizational-tester.cpp
	belcard-rfc6474-tester.cpp
	belcard-security-tester.cpp
	belcard-tester.cpp
	belcard-vcard-tester.cpp
)

apply_compile_flags(SOURCE_FILES_C "CPP" "C")
apply_compile_flags(SOURCE_FILES_CXX "CPP" "CXX")

add_definitions(-DBC_CONFIG_FILE="config.h")

add_executable(belcard-tester ${SOURCE_FILES_C} ${SOURCE_FILES_CXX})
set_target_properties(belcard-tester PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(belcard-tester PUBLIC ${CUNIT_INCLUDE_DIRS} PRIVATE common)
target_link_libraries(belcard-tester belcard ${CUNIT_LIBRARIES})