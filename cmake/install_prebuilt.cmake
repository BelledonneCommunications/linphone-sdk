############################################################################
# install_prebuilt.cmake
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

file(GLOB_RECURSE FILES_TO_INSTALL RELATIVE ${SOURCE_DIR} "${SOURCE_DIR}/*")
foreach(FILE_TO_INSTALL ${FILES_TO_INSTALL})
	get_filename_component(INSTALL_SUBDIR ${FILE_TO_INSTALL} DIRECTORY)
	file(MAKE_DIRECTORY ${INSTALL_SUBDIR})
	file(COPY ${SOURCE_DIR}/${FILE_TO_INSTALL} DESTINATION ${INSTALL_DIR}/${INSTALL_SUBDIR})
endforeach()
