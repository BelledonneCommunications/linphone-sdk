/*
video.cc
Copyright (C) 2016 Belledonne Communications, Grenoble, France 

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at
your option) any later version.

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "commands/video.h"

Video::Video() :
	DaemonCommand("video",
				  "video <call id>",
				  "Toggles camera on current call."
				  "If no call is specified, the current call is taken.")
{
	addExample(new DaemonCommandExample("video 1",
										"Status: Ok\n\n"
										"Camera activated."));

	addExample(new DaemonCommandExample("video 1",
										"Status: Ok\n\n"
										"Camera deactivated."));

	addExample(new DaemonCommandExample("video",
										"Status: Error\n\n"
										"Reason: No current call available."));

	addExample(new DaemonCommandExample("video 2",
										"Status: Error\n\n"
										"Reason: No call with such id."));
}

void Video::exec(Daemon* app, const char* args)
{
	LinphoneCore *lc = app->getCore();
	int cid;
	LinphoneCall *call = NULL;
	bool activate = false;
	int argc = sscanf(args, "%i", &cid);

	if ( argc == 1) { // should take current call
		call = linphone_core_get_current_call(lc);
		if (call == NULL) {
			app->sendResponse(Response("No current call available."));
			return;
		}
	} else {
		call = app->findCall(cid);
		if (call == NULL) {
			app->sendResponse(Response("No call with such id."));
			return;
		}
	}

	if (linphone_call_get_state(call)==LinphoneCallStreamsRunning){
		LinphoneCallParams *new_params = linphone_core_create_call_params(lc, call);
		activate = !linphone_call_params_video_enabled(new_params);

		linphone_call_params_enable_video(new_params,activate);
		linphone_core_update_call(lc,call,new_params);
		linphone_call_params_destroy(new_params);

	} else {
		app->sendResponse(Response("No streams running: can't [de]activate video"));
		return;
	}

	app->sendResponse(Response(activate?"Camera activated.":
										"Camera deactivated", Response::Ok));
}


VideoSource::VideoSource():
	DaemonCommand("videosource",
				  "videosource <cam|dummy> <call-id>",
				  "Toggles camera source for specified call."
				  "If no call is specified, the current call is taken.")
{
	addExample(new DaemonCommandExample("videosource cam 1",
										"Status: Ok\n\n"
										"Webcam source selected."));

	addExample(new DaemonCommandExample("videosource dummy 1",
										"Status: Ok\n\n"
										"Dummy source selected."));

	addExample(new DaemonCommandExample("videosource cam",
										"Status: Error\n\n"
										"Reason: No current call available."));

	addExample(new DaemonCommandExample("videosource cam 2",
										"Status: Error\n\n"
										"Reason: No call with such id."));
}

void VideoSource::exec(Daemon* app, const char* args)
{
	LinphoneCore *lc = app->getCore();
	LinphoneCall *call = NULL;
	int cid;
	int argc = 0;
	bool activate = false;
	char command[6];

	argc = sscanf(args, "%5s %i", command, &cid);
	command[5] = 0;

	if ( argc == 1) { // should take current call
		call = linphone_core_get_current_call(lc);
		if (call == NULL) {
			app->sendResponse(Response("No current call available."));
			return;
		}
	} else if( argc == 2 ) {
		call = app->findCall(cid);
		if (call == NULL) {
			app->sendResponse(Response("No call with such id."));
			return;
		}
	} else {
		app->sendResponse(Response("Invalid command"));
		return;
	}

	activate = (strcmp(command,"cam") == 0);

	linphone_call_enable_camera(call,activate);

	app->sendResponse(Response(activate?"Dummy source selected.":
										"Webcam source selected.", Response::Ok));
}


AutoVideo::AutoVideo():
	DaemonCommand("autovideo",
				  "autovideo <on|off>",
				  "Enables/disables automatic video setup when a call is issued")
{
	addExample(new DaemonCommandExample("autovideo on",
										"Status: Ok\n\n"
										"Auto video ON"));

	addExample(new DaemonCommandExample("autovideo off",
										"Status: Ok\n\n"
										"Auto video OFF"));
}

void AutoVideo::exec(Daemon* app, const char* args)
{
	bool enable = (strcmp(args,"on") == 0);

	app->setAutoVideo(enable);
	app->sendResponse(Response(enable?"Auto video ON":
										"Auto video OFF", Response::Ok));
}
