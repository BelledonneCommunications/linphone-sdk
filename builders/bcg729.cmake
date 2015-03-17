############################################################################
# bcg729.cmake
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

set(EP_bcg729_GIT_REPOSITORY "git://git.linphone.org/bcg729.git")
set(EP_bcg729_GIT_TAG_LATEST "master")
set(EP_bcg729_GIT_TAG "b2c07fe46ab131fe9a8db40397865c70d6f4cb7d")

set(EP_linphone_CMAKE_OPTIONS )
set(EP_linphone_LINKING_TYPE "-DENABLE_STATIC=NO")
set(EP_bcg729_DEPENDENCIES EP_ms2)
