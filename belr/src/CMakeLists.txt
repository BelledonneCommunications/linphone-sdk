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

set(BELR_HEADER_FILES )
set(BELR_SOURCE_FILES_C )
set(BELR_SOURCE_FILES_CXX 
	abnf.cc
	belr.cc
	grammarbuilder.cc
	parser.cc
)

apply_compile_flags(BELR_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(BELR_SOURCE_FILES_CXX "CPP" "CXX")

if(ENABLE_STATIC)
	add_library(belr STATIC ${BELR_HEADER_FILES} ${BELR_SOURCE_FILES_C} ${BELR_SOURCE_FILES_CXX})
else()
	add_library(belr SHARED ${BELR_HEADER_FILES} ${BELR_SOURCE_FILES_C} ${BELR_SOURCE_FILES_CXX})
	set_target_properties(belr PROPERTIES VERSION ${BELR_SO_VERSION})
endif()
target_link_libraries(belr)

install(TARGETS belr EXPORT ${EXPORT_TARGETS_NAME}Targets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES ${BELR_HEADER_FILES}
	DESTINATION include/belr
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
