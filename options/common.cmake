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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Common build options

lcb_add_option("Unit tests" "Enable unit tests support with BCUnit library." "${DEFAULT_VALUE_ENABLE_UNIT_TESTS}")
lcb_add_option("Debug logs" "Enable debug level logs in libinphone and mediastreamer2." NO)
lcb_add_option("Doc" "Enable documentation generation with Doxygen." YES)
lcb_add_option("Tools" "Enable tools binary compilation." "${DEFAULT_VALUE_ENABLE_TOOLS}")
lcb_add_option("unmaintained" "Allow inclusion of unmaintained code in the build." OFF)
