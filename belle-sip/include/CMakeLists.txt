############################################################################
# CMakeLists.txt
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

set(HEADER_FILES
	auth-helper.h
	belle-sdp.h
	belle-sip.h
	bodyhandler.h
	defs.h
	dialog.h
	dict.h
	generic-uri.h
	headers.h
	http-listener.h
	http-message.h
	http-provider.h
	list.h
	listener.h
	listeningpoint.h
	mainloop.h
	message.h
	object.h
	parameters.h
	provider.h
	refresher.h
	resolver.h
	sipstack.h
	sip-uri.h
	transaction.h
	types.h
	utils.h
	wakelock.h
)

set(BELLE_SIP_HEADER_FILES )
foreach(HEADER_FILE ${HEADER_FILES})
	list(APPEND BELLE_SIP_HEADER_FILES "${CMAKE_CURRENT_LIST_DIR}/belle-sip/${HEADER_FILE}")
endforeach()
set(BELLE_SIP_HEADER_FILES ${BELLE_SIP_HEADER_FILES} PARENT_SCOPE)

install(FILES ${BELLE_SIP_HEADER_FILES}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/belle-sip
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
