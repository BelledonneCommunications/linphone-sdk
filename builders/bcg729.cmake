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
set(EP_bcg729_GIT_TAG "133ce9d96a1384efb6e3716ec13e32a563db7e93")

set(EP_linphone_CMAKE_OPTIONS )
set(EP_linphone_LINKING_TYPE "-DENABLE_STATIC=NO")
set(EP_bcg729_DEPENDENCIES EP_ms2)
