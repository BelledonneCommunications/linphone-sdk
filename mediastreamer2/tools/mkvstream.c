/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2015  Belledonne Communications SARL

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/



#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mseventqueue.h"
#include <signal.h>

static int active = 1;

/*handler called when pressing C-c*/
static void stop_handler(int signum){
	active--;
	if (active<0) {
		ms_error("Brutal exit (%d)\n", active);
		exit(-1);
	}
}

static void usage(const char *prog){
	fprintf(stderr, "Usage:\n%s play <mkv file> <dest_ip> <dest_port>\n"
			"%s record <mkv file> local_port\n", prog, prog); 
	exit(-1);
}

typedef enum {
	PLAY_MODE,
	RECORD_MODE,
	INVALID_MODE
}Mode;

/*the payload type number we're using here in the RTP packets sent and received*/
const int payload_type_number = 114;

/*callback function in which we are notified of end of file when playing an mkvfile to rtp*/
static void on_end_of_play(void *user_data, MSFilter *player, unsigned int event_id, void *event_arg){
	switch(event_id){
		case MS_PLAYER_EOF:
			fprintf(stdout, "End of file reached.\n");
			/*make the program exit*/
			active = 0;
		break;
	}
}

/*
 * This small program starts a video stream to either
 * - read an H264 video track from mkv file and stream it out with RTP to specified destination
 * - receive H264 RTP packets on a local port and record them into an mkv file
 */
int main(int argc, char *argv[]){
	const char *command;
	const char *file;
	const char *ip;
	int port;
	VideoStream *stream;
	RtpProfile *profile;
	PayloadType *pt;
	Mode mode = INVALID_MODE;
	int local_port = 7778;
	MSFactory *factory ;
	
	MSMediaStreamIO io = MS_MEDIA_STREAM_IO_INITIALIZER;
	int err;
	
	/*parse command line arguments*/
	
	if (argc<4) usage(argv[0]);
	
	command = argv[1];
	if (strcasecmp(command,"play")==0) mode = PLAY_MODE;
	else if (strcasecmp(command, "record")==0) mode = RECORD_MODE;
	else usage(argv[0]);
	
	file = argv[2];
	
	if (mode == PLAY_MODE){
		ip = argv[3];
		if (argc<5) usage(argv[0]);
		port = atoi(argv[4]);
	}else{
		local_port = atoi(argv[3]);
		ip = "127.0.0.1"; port = 9990; /*dummy destination address, we won't send anything here anyway*/
	}
	
	/*set a signal handler to interrupt the program cleanly*/
	signal(SIGINT,stop_handler);
	
	/*initialize mediastreamer2*/
	factory = ms_factory_new_with_voip();
	
	/*create the video stream */
	stream = video_stream_new(factory, local_port, local_port+1, FALSE);
	
	/*define its local input and outputs with the MSMediaStreamIO structure*/
	if (mode == PLAY_MODE){
		io.input.type = MSResourceFile;
		io.input.file = file; /*the file we want to stream out via rtp*/
		io.output.type = MSResourceFile;
		io.output.file = NULL; /*we don't set a record file in PLAY_MODE, we just want the received video stream to be ignored, if something is received*/
	}else{
		io.input.type = MSResourceFile;
		io.input.file = NULL; /*We don't want to send anything via RTP in RECORD_MODE*/
		io.output.type = MSResourceFile;
		io.output.file = file; /*The file to which we want to record the received video stream*/
	}
	
	/*define the RTP profile to use: in this case we just want to use H264 codec*/
	profile = rtp_profile_new("My RTP profile");
	pt = payload_type_clone(&payload_type_h264);
	rtp_profile_set_payload(profile, payload_type_number, pt); /*we assign H264 to payload type number payload_type_number*/
	
	media_stream_set_target_network_bitrate(&stream->ms, 500000); /*set a target IP bitrate in bits/second */
	
	/*By default, the VideoStream will show up a display window where the received video is played, with a local preview as well.
	 * If you don't need this, assign (void*)-1 as window id, which explicitely disable the display feature.*/
	
	/*video_stream_set_native_window_id(stream, (void*)-1);*/
	
	/*start the video stream, given the RtpProfile and "io" definition */
	err = video_stream_start_from_io(stream, profile, ip, port, ip, port+1, payload_type_number, &io);
	if (err !=0 ){
		fprintf(stderr,"Could not start video stream.");
		goto end;
	}
	/*Register an event handler on the player to be notified of end of file*/
	ms_filter_add_notify_callback(stream->source, on_end_of_play, NULL, FALSE);
	
	/*program's main loop*/
	while (active){
		/*handle video stream background activity. This is non blocking*/
		video_stream_iterate(stream);
		/*process event callbacks*/
		ms_event_queue_pump(ms_factory_get_event_queue(factory));
		ms_usleep(50000); /*pause 50ms to avoid busy loop*/
	}
	
end:
	/*stop and destroy the video stream object*/
	if (stream) video_stream_stop(stream);
	/*free the RTP profile and payload type inside*/
	if (profile) rtp_profile_destroy(profile);
	
	ms_factory_destroy(factory);
	
	return err;
}
