############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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

set(DOC_FILES
	BCUnit_doc.css
	error_handling.html
	fdl.html
	index.html
	introduction.html
	managing_tests.html
	running_tests.html
	test_registry.html
	writing_tests.html
)

install(FILES ${DOC_FILES}
	DESTINATION share/doc/BCUnit
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)


file(GLOB HEADER_FILES "../BCUnit/Headers/*.h")
list(APPEND HEADER_FILES "../BCUnit/Sources/Win/Win.h")

install(FILES ${HEADER_FILES}
	DESTINATION share/doc/BCUnit/headers
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
