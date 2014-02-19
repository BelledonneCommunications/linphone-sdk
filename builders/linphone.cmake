############################################################################
# linphone.cmake
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

set(EP_linphone_GIT_REPOSITORY "git://git.linphone.org/linphone.git")
set(EP_linphone_GIT_TAG "60f6726c61d463c43e6038b629628f509add2e3c") # Branch 'master'
set(EP_linphone_AUTOTOOLS "yes")
set(EP_linphone_CONFIGURE_OPTIONS "--disable-strict --enable-bellesip --enable-external-ortp --enable-external-mediastreamer")
set(EP_linphone_DEPENDENCIES EP_bellesip EP_ortp EP_ms2 EP_xml2)

if(${ENABLE_ZRTP})
	# TODO
else(${ENABLE_ZRTP})
	set(EP_linphone_CONFIGURE_OPTIONS "${EP_linphone_CONFIGURE_OPTIONS} --disable-zrtp")
endif(${ENABLE_ZRTP})

if(${ENABLE_UNIT_TESTS})
	list(APPEND EP_linphone_DEPENDENCIES EP_cunit)
else(${ENABLE_UNIT_TESTS})
	set(EP_linphone_CONFIGURE_OPTIONS "${EP_linphone_CONFIGURE_OPTIONS} --disable-tests")
endif(${ENABLE_UNIT_TESTS})
