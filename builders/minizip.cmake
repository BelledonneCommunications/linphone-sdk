############################################################################
# minizip.cmake
# Copyright (C) 2018  Belledonne Communications, Grenoble France
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

lcb_git_repository("https://gitlab.linphone.org/BC/public/external/minizip.git")
lcb_git_tag_latest("master")
lcb_git_tag("d65cd2ea9d740f62884e0beaf8ab86740620c783")
lcb_external_source_paths("externals/minizip")
lcb_spec_file("minizip.spec")

lcb_dependencies("zlib")

lcb_cmake_options(
	"-DUSE_AES=NO"
)

