############################################################################
# CMakeLists.txt
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

set(HEADER_FILES
	allfilters.h
	bitratecontrol.h
	bits_rw.h
	devices.h
	dsptools.h
	dtls_srtp.h
	dtmfgen.h
	flowcontrol.h
	formats.h
	ice.h
	mediastream.h
	ms_srtp.h
	msaudiomixer.h
	mschanadapter.h
	mscodecutils.h
	mscommon.h
	msconference.h
	msequalizer.h
	mseventqueue.h
	msextdisplay.h
	msfactory.h
	msfileplayer.h
	msfilerec.h
	msfilter.h
	msgenericplc.h
	msinterfaces.h
	msitc.h
	msjava.h
	msjpegwriter.h
	msmediaplayer.h
	msqueue.h
	msrtp.h
	mssndcard.h
	mstee.h
	msticker.h
	mstonedetector.h
	msutils.h
	msv4l.h
	msvaddtx.h
	msvideo.h
	msvideoout.h
	msvideopresets.h
	msvolume.h
	mswebcam.h
	qualityindicator.h
	rfc3984.h
	stun.h
	upnp_igd.h
	x11_helper.h
	zrtp.h
	msrtt4103.h
	msasync.h
	msudp.h
	mspcapfileplayer.h
)

set(MEDIASTREAMER2_HEADER_FILES )
foreach(HEADER_FILE ${HEADER_FILES})
	list(APPEND MEDIASTREAMER2_HEADER_FILES "${CMAKE_CURRENT_LIST_DIR}/mediastreamer2/${HEADER_FILE}")
endforeach()
set(MEDIASTREAMER2_HEADER_FILES ${MEDIASTREAMER2_HEADER_FILES} PARENT_SCOPE)

install(FILES ${MEDIASTREAMER2_HEADER_FILES}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mediastreamer2
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
