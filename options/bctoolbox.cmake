############################################################################
# bctoolbox.cmake
# Copyright (C) 2016  Belledonne Communications, Grenoble France
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

# bctoolbox build options

option(ENABLE_POLARSSL "Enable polarssl support." ${DEFAULT_VALUE_ENABLE_POLARSSL})
add_feature_info("Polarssl" ENABLE_POLARSSL "Crypto stack implementation based on polarssl")
option(ENABLE_MBEDTLS "Enable mbedtls support." ${DEFAULT_VALUE_ENABLE_MBEDTLS})
add_feature_info("Mbedtls" ENABLE_MBEDTLS "Crypto stack implementation based on mbeddtls")
