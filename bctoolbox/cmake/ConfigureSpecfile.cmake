############################################################################
# ConfigureSpecfile.cmake
# Copyright (C) 2010-2019  Belledonne Communications, Grenoble France
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

include("${BCTOOLBOX_CMAKE_UTILS}")

set(FULL_VERSION )
bc_compute_full_version(FULL_VERSION)

set(version_major )
set(version_minor )
set(version_patch )
set(identifiers )
set(metadata )

bc_parse_full_version("${FULL_VERSION}" version_major version_minor version_patch identifiers metadata)

set(RPM_VERSION ${version_major}.${version_minor}.${version_patch})
if (NOT identifiers)
	set(RPM_RELEASE 1)
else()
	string(SUBSTRING "${identifiers}" 1 -1 identifiers)
	set(RPM_RELEASE "0.${identifiers}${metadata}")
endif()

configure_file("${SRC}" "${DEST}")
