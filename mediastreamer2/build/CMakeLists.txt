############################################################################
# Copyright (c) 2010-2023 Belledonne Communications SARL.
#
# This file is part of mediastreamer2 
# (see https://gitlab.linphone.org/BC/public/mediastreamer2).
#
############################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################

if(NOT CPACK_PACKAGE_NAME)
	string(TOLOWER "${PROJECT_NAME}" LOWERCASE_PROJECT_NAME)
	set(CPACK_PACKAGE_NAME "${LOWERCASE_PROJECT_NAME}")
endif()

set(CPACK_SOURCE_IGNORE_FILES
	"${CMAKE_BINARY_DIR}"
	"^${PROJECT_SOURCE_DIR}/.git*"
)

bc_make_package_source_target()
