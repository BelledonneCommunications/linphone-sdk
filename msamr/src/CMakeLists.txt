############################################################################
# CMakeLists.txt
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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

set(LIBS )

set(SOURCE_FILES msamr.c)
if(ENABLE_NARROWBAND)
	list(APPEND SOURCE_FILES amrnb.c)
	list(APPEND LIBS ${OpenCoreAMRNB_TARGET})
endif()
if(ENABLE_WIDEBAND)
	list(APPEND SOURCE_FILES amrwb.c)
	list(APPEND LIBS ${OpenCoreAMRWB_TARGET} ${VoAMRWBEnc_TARGET})
endif()

if(BUILD_SHARED_LIBS)
	if(IOS)
		add_library(msamr SHARED ${SOURCE_FILES})
	else()
		add_library(msamr MODULE ${SOURCE_FILES})
	endif()
else()
	add_library(msamr STATIC ${SOURCE_FILES})
endif()

if(ENABLE_NARROWBAND)
	target_compile_definitions(msamr PRIVATE "HAVE_AMRNB=1")
endif()
if(ENABLE_WIDEBAND)
	target_compile_definitions(msamr PRIVATE "HAVE_AMRWB=1")
endif()
target_compile_definitions(msamr PRIVATE "HAVE_CONFIG_H")
target_link_libraries(msamr PRIVATE ${Mediastreamer2_TARGET} ${LIBS})
if(OpenCoreAMRNB_USE_BUILD_INTERFACE)
	target_compile_definitions(msamr PRIVATE OPENCOREAMRNB_USE_BUILD_INTERFACE)
endif()
if(OpenCoreAMRWB_USE_BUILD_INTERFACE)
	target_compile_definitions(msamr PRIVATE OPENCOREAMRWB_USE_BUILD_INTERFACE)
endif()
if(VoAMRWBEnc_USE_BUILD_INTERFACE)
	target_compile_definitions(msamr PRIVATE VOAMRWBENC_USE_BUILD_INTERFACE)
endif()

if(BUILD_SHARED_LIBS)
	if(APPLE)
		if(IOS)
			set_target_properties(msamr PROPERTIES
				FRAMEWORK TRUE
				MACOSX_FRAMEWORK_IDENTIFIER org.linphone.msamr
				MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
			)
		endif()
	endif()
	if(MSVC)
		install(FILES $<TARGET_PDB_FILE:msamr>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
		set_target_properties(msamr PROPERTIES PREFIX "lib")
	endif()
endif()


install(TARGETS msamr
	RUNTIME DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	LIBRARY DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	ARCHIVE DESTINATION "${Mediastreamer2_PLUGINS_DIR}"
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
