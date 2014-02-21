############################################################################
# ortp.cmake
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

set(EP_ortp_GIT_REPOSITORY "git://git.linphone.org/ortp.git")
set(EP_ortp_GIT_TAG "e33c53dfaa387aa77f037a33e8b3b3104b8f852a") # Branch 'master'
set(EP_ortp_USE_AUTOTOOLS "yes")
set(EP_ortp_USE_AUTOGEN "yes")
set(EP_ortp_CONFIG_H_FILE ortp-config.h)
set(EP_ortp_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_TOOLCHAIN_HOST}"
)
set(EP_ortp_CONFIGURE_OPTIONS
	"--disable-strict"
)
set(EP_ortp_LINKING_TYPE "--disable-static" "--enable-shared")
set(EP_ortp_DEPENDENCIES )

if(${ENABLE_SRTP})
	list(APPEND EP_ortp_CONFIGURE_OPTIONS "--with-srtp=${CMAKE_INSTALL_PREFIX}")
	list(APPEND EP_ortp_DEPENDENCIES EP_srtp)
endif(${ENABLE_SRTP})

if(${ENABLE_ZRTP})
	# TODO
else(${ENABLE_ZRTP})
	list(APPEND EP_ortp_CONFIGURE_OPTIONS "--disable-zrtp")
endif(${ENABLE_ZRTP})
