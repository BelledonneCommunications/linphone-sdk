############################################################################
# common.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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

# Common build options

option(ENABLE_UNIT_TESTS "Enable unit tests support with CUnit library." "${DEFAULT_VALUE_ENABLE_UNIT_TESTS}")
add_feature_info("Unit tests" ENABLE_UNIT_TESTS "Build unit tests programs for belle-sip, mediastreamer2 and linphone.")
option(ENABLE_DEBUG_LOGS "Enable debug level logs in libinphone and mediastreamer2." NO)

option(ENABLE_PACKAGING "Enable packaging" "${DEFAULT_VALUE_ENABLE_PACKAGING}")

option(ENABLE_DOC "Enable documentation generation with Doxygen." YES)
add_feature_info("Documentation" ENABLE_DOC "Enable documentation generation with Doxygen.")
