############################################################################
# Copyright (c) 2010-2022 Belledonne Communications SARL.
#
# This file is part of mediastreamer2 
# (see https://gitlab.linphone.org/BC/public/mediastreamer2).
#
############################################################################
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################

set(HEADER_FILES
	allfilters.h
	android_utils.h
	bitratecontrol.h
	bits_rw.h
	box-plot.h
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
	msanalysedisplay.h
	msfactory.h
	msfileplayer.h
	msfilerec.h
	msfilter.h
	msgenericplc.h
	msinterfaces.h
	msitc.h
	msjava.h
	msjpegwriter.h
	msogl.h
	msogl_functions.h
	msqrcodereader.h
	msmediaplayer.h
	msmediarecorder.h
	msqueue.h
	msrtp.h
	msscreensharing.h
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
	msvideoqualitycontroller.h
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
	msanalysedisplay.h
	msmire.h
	mspacketrouter.h
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
if(ENABLE_VIDEO)
	install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/OpenGL
		DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

endif()
