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

set(LIME_HEADER_FILES
	lime_settings.hpp
	lime_defines.hpp
	lime_keys.hpp
	lime_impl.hpp
	lime_x3dh_protocol.hpp
	lime_localStorage.hpp
	lime_double_ratchet.hpp
	lime_double_ratchet_protocol.hpp
	lime_lime.hpp
	lime_crypto_primitives.hpp
	lime_log.hpp
)
set(LIME_SOURCE_FILES_CXX
	lime.cpp
	lime_crypto_primitives.cpp
	lime_x3dh.cpp
	lime_x3dh_protocol.cpp
	lime_localStorage.cpp
	lime_double_ratchet.cpp
	lime_double_ratchet_protocol.cpp
	lime_manager.cpp
)

if (ENABLE_C_INTERFACE)
	set(LIME_SOURCE_FILES_CXX ${LIME_SOURCE_FILES_CXX} lime_ffi.cpp)
endif()

bc_apply_compile_flags(LIME_SOURCE_FILES_CXX STRICT_OPTIONS_CPP STRICT_OPTIONS_CXX)

if(ENABLE_STATIC)
	add_library(lime-static STATIC ${LIME_HEADER_FILES} ${LIME_SOURCE_FILES_CXX})
	set_target_properties(lime-static PROPERTIES OUTPUT_NAME lime)
	target_include_directories(lime-static PUBLIC ${BCTOOLBOX_INCLUDE_DIRS} ${SOCI_INCLUDE_DIRS} ${SOCI_INCLUDE_DIRS}/soci)
	target_link_libraries(lime-static INTERFACE ${BCTOOLBOX_CORE_LIBRARIES} ${SOCI_LIBRARIES} ${SOCI_sqlite3_PLUGIN})
	if(ENABLE_PROFILING)
		set_target_properties(lime-static PROPERTIES LINK_FLAGS "-pg")
	endif()
endif()
if(ENABLE_SHARED)
	add_library(lime SHARED ${LIME_HEADER_FILES} ${LIME_SOURCE_FILES_CXX})
	set_target_properties(lime PROPERTIES VERSION ${LIME_SO_VERSION})
	target_include_directories(lime PUBLIC ${BCTOOLBOX_INCLUDE_DIRS} ${SOCI_INCLUDE_DIRS} ${SOCI_INCLUDE_DIRS}/soci)
	target_link_libraries(lime PRIVATE ${BCTOOLBOX_CORE_LIBRARIES} ${SOCI_LIBRARIES} ${SOCI_sqlite3_PLUGIN})
	# TODO: replace by if(APPLE) when we want to make apple framework on linphone-desktop too
	if(IOS)
		if(IOS)
			set(MIN_OS ${LINPHONE_IOS_DEPLOYMENT_TARGET})
		else()
			set(MIN_OS ${CMAKE_OSX_DEPLOYMENT_TARGET})
		endif()
		set_target_properties(lime PROPERTIES
			FRAMEWORK TRUE
			MACOSX_FRAMEWORK_IDENTIFIER com.belledonne-communications.lime
			MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_SOURCE_DIR}/build/osx/Info.plist.in"
			PUBLIC_HEADER "${LIME_HEADER_FILES}"
		)
	endif()
	if(WIN32)
		set_target_properties(lime PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
	endif()
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/lime.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()

if(ENABLE_STATIC)
	install(TARGETS lime-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	install(TARGETS lime EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
