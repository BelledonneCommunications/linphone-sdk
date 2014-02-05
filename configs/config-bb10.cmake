############################################################################
# config-bb10.cmake
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

# bellesip
set(EP_bellesip_EXTRA_CFLAGS "-Wno-error=pragmas")

# opus
set(EP_opus_CONFIGURE_OPTIONS "${EP_opus_CONFIGURE_OPTIONS} --enable-fixed-point --disable-asm")

# linphone
set(EP_linphone_CONFIGURE_OPTIONS "${EP_linphone_CONFIGURE_OPTIONS} --disable-nls --with-readline=none --enable-gtk_ui=no --enable-console_ui=no --disable-theora --disable-sdl --disable-x11 --disable-tutorials --disable-tools --disable-msg-storage --disable-video --disable-zrtp --enable-broken-srtp --disable-alsa")
