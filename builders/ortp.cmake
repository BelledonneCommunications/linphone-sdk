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
set(EP_ortp_GIT_TAG "e1ea9d5121cdabbcc16ffdb884bf705caacd81a1") # Branch 'master'
set(EP_ortp_AUTOTOOLS "yes")
set(EP_ortp_CONFIGURE_OPTIONS "--disable-strict")
set(EP_ortp_DEPENDENCIES )

if(${ENABLE_SRTP})
	set(EP_ortp_CONFIGURE_OPTIONS "${EP_ortp_CONFIGURE_OPTIONS} --with-srtp=${CMAKE_INSTALL_PREFIX}")
	list(APPEND EP_ortp_DEPENDENCIES EP_srtp)
endif(${ENABLE_SRTP})

if(${ENABLE_ZRTP})
	# TODO
else(${ENABLE_ZRTP})
	set(EP_ortp_CONFIGURE_OPTIONS "${EP_ortp_CONFIGURE_OPTIONS} --disable-zrtp")
endif(${ENABLE_ZRTP})
