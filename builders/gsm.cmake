############################################################################
# gsm.cmake
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

set(EP_gsm_GIT_REPOSITORY "git://git.linphone.org/gsm.git")
set(EP_gsm_GIT_TAG_LATEST "linphone")
set(EP_gsm_GIT_TAG "b6b37996357ae9d9596b8ee227f33744889d895c")
set(EP_gsm_EXTERNAL_SOURCE_PATHS "gsm" "externals/gsm")

set(EP_gsm_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
if(MSVC)
	set(EP_gsm_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()

