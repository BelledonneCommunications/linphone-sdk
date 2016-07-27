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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

set(BCTOOLBOX_HEADER_FILES
	bctoolbox/crypto.h
	bctoolbox/list.h
	bctoolbox/logging.h
	bctoolbox/map.h
	bctoolbox/port.h
	bctoolbox/vfs.h
)
if(ENABLE_TESTS_COMPONENT)
	list(APPEND BCTOOLBOX_HEADER_FILES bctoolbox/tester.h)
endif()

install(FILES ${BCTOOLBOX_HEADER_FILES}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bctoolbox
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
