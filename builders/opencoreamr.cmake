############################################################################
# opencoreamr.cmake
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

set(EP_opencoreamr_URL "http://downloads.sourceforge.net/project/opencore-amr/opencore-amr/opencore-amr-0.1.3.tar.gz")
set(EP_opencoreamr_BUILD_METHOD "autotools")
set(EP_opencoreamr_USE_AUTOGEN "yes")
set(EP_opencoreamr_CONFIGURE_OPTIONS )
set(EP_opencoreamr_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_TOOLCHAIN_HOST}"
)
set(EP_opencoreamr_LINKING_TYPE "--disable-static" "--enable-shared")

if(${ENABLE_AMRNB})
	list(APPEND EP_opencoreamr_CONFIGURE_OPTIONS
		"--enable-amrnb-decoder" "--enable-amrnb-encoder"
	)
else(${ENABLE_AMRNB})
	list(APPEND EP_opencoreamr_CONFIGURE_OPTIONS
		"--disable-amrnb-decoder" "--disable-amrnb-encoder"
	)
endif(${ENABLE_AMRNB})
