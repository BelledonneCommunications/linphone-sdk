############################################################################
# tunnel.cmake
# Copyright (C) 2014-2018  Belledonne Communications, Grenoble France
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

lcb_git_repository("gitosis@git.linphone.org:tunnel.git")
lcb_git_tag_latest("master")
lcb_git_tag("5d8582208f37174ded18280c7c1f8feefb1c4c55")
lcb_external_source_paths("tunnel")
lcb_groupable(YES)

lcb_dependencies("bctoolbox")
lcb_cmake_options("-DENABLE_SERVERS=NO")

