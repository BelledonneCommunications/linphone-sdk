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
	lime_crypto-tester.cpp
	lime_massive_group-tester.cpp
)

set(SOURCE_FILES_C
	lime_ffi-tester.c
)

bc_apply_compile_flags(SOURCE_FILES_C STRICT_OPTIONS_CPP STRICT_OPTIONS_C)
bc_apply_compile_flags(SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

if(ANDROID OR IOS)
	add_library(limetester SHARED ${HEADER_FILES_CXX} ${SOURCE_FILES_CXX} ${SOURCE_FILES_C} )
	target_include_directories(limetester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS} ${BELLESIP_INCLUDE_DIRS})
	target_link_libraries(limetester ${LIME_LIBRARIES_FOR_TESTER} ${BCTOOLBOX_LIBRARIES} ${BCTOOLBOX_TESTER_LIBRARIES} ${BELLESIP_LIBRARIES} ${SOCI_LIBRARIES} ${SOCI_sqlite3_PLUGIN})
	if(IOS)
		target_link_libraries(limetester sqlite3)
		set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		set_target_properties(limetester PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER com.belledonne-communications.limetester
			MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_SOURCE_DIR}/build/osx/Info.plist.in"
			PUBLIC_HEADER "${HEADER_FILES_CXX}"
		)
	endif()
	install(TARGETS limetester
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
		)
	install(FILES ${HEADER_FILES_CXX}
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lime
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	)
else()
	add_executable(lime_tester ${SOURCE_FILES_CXX} ${HEADER_FILES_CXX} ${SOURCE_FILES_C} )
	set_target_properties(lime_tester PROPERTIES LINKER_LANGUAGE CXX)
	target_include_directories(lime_tester PUBLIC ${BCTOOLBOX_TESTER_INCLUDE_DIRS} ${BELLESIP_INCLUDE_DIRS})
	target_link_libraries(lime_tester ${LIME_LIBRARIES_FOR_TESTER} ${BCTOOLBOX_LIBRARIES} ${BCTOOLBOX_TESTER_LIBRARIES} ${BELLESIP_LIBRARIES} ${SOCI_LIBRARIES} ${SOCI_sqlite3_PLUGIN})

	# lime test suit request a local server to be launched, so keep testing double ratchet only for now
	add_test(NAME double_ratchet COMMAND lime_tester --verbose --resource-dir ${CMAKE_CURRENT_SOURCE_DIR} --suite "double ratchet")

	if(ENABLE_PROFILING)
		set_target_properties(lime_tester PROPERTIES LINK_FLAGS "-pg")
	endif()
endif()

