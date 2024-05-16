############################################################################
# CMakeLists.txt
# Copyright (C) 2015-2023  Belledonne Communications, Grenoble France
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

set(BELR_LIBRARIES_FOR_TESTER belr)

set(GRAMMAR_FILES
	res/basicgrammar.txt
	res/vcardgrammar.txt
	res/sipgrammar.txt
	res/response.txt
	res/register.txt
)

set(BINARY_GRAMMAR_FILES res/belr-grammar-example.blr)

set(HEADER_FILES_CXX belr-tester.h)
set(SOURCE_FILES_CXX
	belr-tester.cpp
	grammar-tester.cpp
	parser.cpp
)

bc_apply_compile_flags(SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

add_executable(belr-tester ${SOURCE_FILES_CXX} ${HEADER_FILES_CXX})
set_target_properties(belr-tester PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(belr-tester PUBLIC ${BCTOOLBOX_INCLUDE_DIRS})
target_link_libraries(belr-tester PRIVATE belr ${BCToolbox_tester_TARGET})

if(NOT IOS)
	install(TARGETS belr-tester
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()

install(FILES ${GRAMMAR_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/belr-tester/res")
install(FILES ${BINARY_GRAMMAR_FILES} DESTINATION "${CMAKE_INSTALL_DATADIR}/belr/grammars")

