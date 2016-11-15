############################################################################
# bcunit.cmake
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

lcb_git_repository("git://git.linphone.org/bcunit.git")
lcb_git_tag_latest("linphone")
lcb_git_tag("0a0a9c60f5a1b899ae26b705fa5224ef25377982")
lcb_external_source_paths("bcunit")
lcb_ignore_warnings(YES)

lcb_cmake_options(
	"-DENABLE_AUTOMATED=YES"
	"-DENABLE_CONSOLE=NO"
)
