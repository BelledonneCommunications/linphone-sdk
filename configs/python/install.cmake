############################################################################
# install.cmake
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


file(GLOB EXE_FILEPATH "${BINARY_DIR}/build_exe/linphone/dist/${PACKAGE_NAME}-*.exe")
file(GLOB MSI_FILEPATH "${BINARY_DIR}/build_msi/linphone/dist/${PACKAGE_NAME}-*.msi")
file(GLOB ZIP_FILEPATH "${BINARY_DIR}/build_zip/linphone/dist/${PACKAGE_NAME}-*.zip")
file(GLOB WHL_FILEPATH "${BINARY_DIR}/build_wheel/linphone/dist/${PACKAGE_NAME}-*.whl")

string(REPLACE "." "" PYTHON_VERSION_NO_DOT "${PYTHON_VERSION}")
string(REPLACE "-" "_" PLATFORM_TAG_UNDERSCORED "${PLATFORM_TAG}")

if(EXE_FILEPATH)
	get_filename_component(EXE_FILENAME "${EXE_FILEPATH}" NAME)
	file(COPY "${EXE_FILEPATH}" DESTINATION "${INSTALL_DIR}")
	file(RENAME "${INSTALL_DIR}/${EXE_FILENAME}" "${INSTALL_DIR}/${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${PLATFORM_TAG}-py${PYTHON_VERSION}.exe")
endif()
if(MSI_FILEPATH)
	get_filename_component(MSI_FILENAME "${MSI_FILEPATH}" NAME)
	file(COPY "${MSI_FILEPATH}" DESTINATION "${INSTALL_DIR}")
	file(RENAME "${INSTALL_DIR}/${MSI_FILENAME}" "${INSTALL_DIR}/${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${PLATFORM_TAG}-py${PYTHON_VERSION}.msi")
endif()
if(ZIP_FILEPATH)
	get_filename_component(ZIP_FILENAME "${ZIP_FILEPATH}" NAME)
	file(COPY "${ZIP_FILEPATH}" DESTINATION "${INSTALL_DIR}")
	file(RENAME "${INSTALL_DIR}/${ZIP_FILENAME}" "${INSTALL_DIR}/${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}.${PLATFORM_TAG}-py${PYTHON_VERSION}.zip")
endif()
if(WHL_FILEPATH)
	get_filename_component(WHL_FILENAME "${WHL_FILEPATH}" NAME)
	file(COPY "${WHL_FILEPATH}" DESTINATION "${INSTALL_DIR}")
	file(RENAME "${INSTALL_DIR}/${WHL_FILENAME}" "${INSTALL_DIR}/${PACKAGE_NAME}-${LINPHONE_GIT_REVISION}-py${PYTHON_VERSION_NO_DOT}-none-${PLATFORM_TAG_UNDERSCORED}.whl")
endif()
