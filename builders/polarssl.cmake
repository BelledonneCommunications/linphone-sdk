############################################################################
# polarssl.cmake
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

lcb_git_repository("git://git.linphone.org/polarssl.git")
lcb_git_tag_latest("linphone-1.4")
lcb_git_tag("3b7c2443e75e51b7af67a3e5dcb3771ae3120ff3")
lcb_external_source_paths("polarssl" "externals/polarssl")
lcb_may_be_found_on_system(YES)
lcb_ignore_warnings(YES)

lcb_linking_type("-DUSE_SHARED_POLARSSL_LIBRARY=1")
lcb_cmake_options("-DENABLE_PROGRAMS=0" "-DENABLE_TESTING=0")
