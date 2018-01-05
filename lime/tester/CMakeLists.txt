############################################################################
# CMakeLists.txt
# Copyright (C) 2017  Belledonne Communications, Grenoble France
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

if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_bellesip_CONFIG_DIR}/BelleSIPConfig.cmake")
else()
	find_package(BelleSIP REQUIRED)
endif()

if(ENABLE_SHARED)
	set(LIME_LIBRARIES_FOR_TESTER lime)
else()
	set(LIME_LIBRARIES_FOR_TESTER lime-static)
endif()
set(HEADER_FILES_CXX lime-tester.hpp lime-tester-utils.hpp)
set(SOURCE_FILES_CXX
	lime-tester.cpp
	lime-tester-utils.cpp
	lime_double_ratchet-tester.cpp
	lime_lime-tester.cpp
	lime_helloworld-tester.cpp
)

bc_apply_compile_flags(SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

include_directories(${BELLESIP_INCLUDE_DIRS})
add_executable(lime_tester ${SOURCE_FILES_CXX} ${HEADER_FILES_CXX})
set_target_properties(lime_tester PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(lime_tester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS})
target_link_libraries(lime_tester ${LIME_LIBRARIES_FOR_TESTER} ${BCTOOLBOX_LIBRARIES} ${BCTOOLBOX_TESTER_LIBRARIES} ${BELLESIP_LIBRARIES} ${SOCI_LIBRARIES} ${SOCI_sqlite3_PLUGIN} ${SQLITE3_LIBRARIES})

if(APPLE)
	set_target_properties(lime_tester PROPERTIES LINK_FLAGS "-stdlib=libc++")
endif()

# line test suit request a local server to be launched, so keep testing double ratchet only for now
add_test(NAME double_ratchet COMMAND lime_tester --verbose --resource-dir ${CMAKE_CURRENT_SOURCE_DIR} --suite "double ratchet")

if(NOT IOS)
	install(TARGETS lime_tester
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
