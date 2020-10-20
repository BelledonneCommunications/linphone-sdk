/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Convenient API to create and manage audio conferences.
 */

#ifndef msconference_h
#define msconference_h

#include "mediastreamer2/mediastream.h"

/**
 * @addtogroup mediastreamer2_audio_conference
 * @{
 */

struct _MSAudioConference;
struct _MSAudioEndpoint;

typedef void (*MSAudioConferenceNotifyActiveTalker)(struct _MSAudioConference *, struct _MSAudioEndpoint *ep);

/**
 * Structure that holds audio conference parameters
**/
struct _MSAudioConferenceParams{
	int samplerate; /**< Conference audio sampling rate in Hz: 8000, 16000 ...*/
	MSAudioConferenceNotifyActiveTalker active_talker_callback;
	void *user_data;
};

/**
 * Typedef to structure that holds conference parameters
**/
typedef struct _MSAudioConferenceParams MSAudioConferenceParams;

/**
 * The MSAudioConference is the object representing an audio conference.
 *
 * First, the conference has to be created with ms_audio_conference_new(), with parameters supplied.
 * Then, participants to the conference can be added with ms_audio_conference_add_member().
 * The MSAudioConference takes in charge the mixing and dispatching of the audio to the participants.
 * If participants (MSAudioEndpoint) are using sampling rate different from the conference, then sample rate converters are automatically added
 * and configured.
 * Participants can be removed from the conference with ms_audio_conference_remove_member().
 * The conference processing is performed in a new thread run by a MSTicker object, which is owned by the conference.
 * When all participants are removed, the MSAudioConference object can then be safely destroyed with ms_audio_conference_destroy().
**/
typedef struct _MSAudioConference MSAudioConference;


/**
 * The MSAudioEndpoint represents a participant in the conference.
 * It can be constructed from an existing AudioStream object with
 * ms_audio_endpoint_get_from_stream().
**/
typedef struct _MSAudioEndpoint MSAudioEndpoint;



#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a conference.
 * @param params a MSAudioConferenceParams structure, containing conference parameters.
 * @param factory a MSFactory structure, containing filters parameters
 * @return a MSAudioConference object.
**/
MS2_PUBLIC MSAudioConference * ms_audio_conference_new(const MSAudioConferenceParams *params, MSFactory *factory);

/**
 * Gets conference's current parameters.
 * @param obj the conference.
 * @return a read-only pointer to the conference parameters.
**/
MS2_PUBLIC const MSAudioConferenceParams *ms_audio_conference_get_params(MSAudioConference *obj);

/**
 * Adds a participant to the conference.
 * @param obj the conference
 * @param ep the participant, represented as a MSAudioEndpoint object
**/
MS2_PUBLIC void ms_audio_conference_add_member(MSAudioConference *obj, MSAudioEndpoint *ep);

/**
 * Removes a participant from the conference.
 * @param obj the conference
 * @param ep the participant, represented as a MSAudioEndpoint object
**/
MS2_PUBLIC void ms_audio_conference_remove_member(MSAudioConference *obj, MSAudioEndpoint *ep);


/**
 * Process events of the audio conference.
 * Calling this method periodically (for example every 50 ms), is necessary
 * to receive the active talker notifications to the callback set in the MSAudioConferenceParams.
 * @param obj the conference
**/
MS2_PUBLIC void ms_audio_conference_process_events(MSAudioConference *obj);
/**
 * Mutes or unmutes a participant.
 * 
 * @param obj the conference
 * @param ep the participant, represented as a MSAudioEndpoint object
 * @param muted true to mute the participant, false to unmute.
 *
 * By default all participants are unmuted.
**/
MS2_PUBLIC void ms_audio_conference_mute_member(MSAudioConference *obj, MSAudioEndpoint *ep, bool_t muted);

/**
 * Returns the size (ie the number of participants) of a conference.
 * @param obj the conference
**/
MS2_PUBLIC int ms_audio_conference_get_size(MSAudioConference *obj);

/**
 * Destroys a conference.
 * @param obj the conference
 * All participants must have been removed before destroying the conference.
**/
MS2_PUBLIC void ms_audio_conference_destroy(MSAudioConference *obj);

/**
 * Creates an MSAudioEndpoint from an existing AudioStream.
 *
 * In order to create graphs for audio processing of each participant, the AudioStream object is used, because
 * this object already handles all the processing for volume control, encoding, decoding, etc...
 *
 * The construction of the participants depends whether it is a remote participant, that is somebody in the network
 * sending and receiving audio through RTP, or a local participant, that is somebody using the local soundcard to capture
 * and play audio.
 *
 * To create a remote participant, first create and start an AudioStream for the participant with audio_stream_new() and
 * audio_stream_start_with_files(), given NULL arguments as input and output files.
 * This participant does not interact with soundcards, this is why we suggest to use audio_stream_start_full() to avoid 
 * holding any reference to the sound system.
 * Then, create a MSAudioEndpoint representing this participant by calling ms_audio_endpoint_get_from_stream() with
 * is_remote=TRUE.
 *
 * To create a local participant, first create and start an AudioStream with audio_stream_new() and audio_stream_start_full(), 
 * with real soundcard arguments.
 * Arguments controlling RTP should be filled with placeholders value and will not be used for conferencing.
 * Then, create a MSAudioEndpoint representing this local participant by calling ms_audio_endpoint_get_from_stream() 
 * with the audiostream and is_remote=FALSE.<br>
 * For example:<br>
 * <PRE>
 * AudioStream *st=audio_stream_new(65000,65001,FALSE);
 * audio_stream_start_full(st, conf->local_dummy_profile,
 *				"127.0.0.1",
 *				65000,
 *				"127.0.0.1",
 *				65001,
 *				0,
 *				40,
 *				NULL,
 *				NULL,
 *				playcard,
 *				captcard,
 *				needs_echocancellation,
 *				);
 * MSAudioEndpoint *local_endpoint=ms_audio_endpoint_get_from_stream(st,FALSE);
 * </PRE>
**/
MS2_PUBLIC MSAudioEndpoint * ms_audio_endpoint_get_from_stream(AudioStream *st, bool_t is_remote);

/**
 * Associate a user pointer to the endpoint.
 * @param ep the endpoint
 * @param user_data the user data
 */
MS2_PUBLIC void ms_audio_endpoint_set_user_data(MSAudioEndpoint *ep, void *user_data);

/**
 * Get the user pointer associated to the endpoint.
 * @param ep the endpoint
 * @return the user data
 */
MS2_PUBLIC void * ms_audio_endpoint_get_user_data(const MSAudioEndpoint *ep);


/**
 * Destroys a MSAudioEndpoint that was created from an AudioStream with ms_audio_endpoint_get_from_stream().
 * The AudioStream can then be destroyed if needed.
**/
MS2_PUBLIC void ms_audio_endpoint_release_from_stream(MSAudioEndpoint *obj);

/**
 * Creates an audio endpoint (or virtual participant) to record the conference into a wav file.
 * @param factory The factory used by the linphone core.
**/
MS2_PUBLIC MSAudioEndpoint * ms_audio_endpoint_new_recorder(MSFactory* factory);

/**
 * Start audio recording.
 * The endpoint must have been created by ms_audio_endpoint_new_recorder().
 * @param ep the endpoint
 * @param path path for the wav file where to record samples.
 * @return 0 if successful, -1 if the path is invalid.
**/
MS2_PUBLIC int ms_audio_recorder_endpoint_start(MSAudioEndpoint *ep, const char *path);

/**
 * Stop audio recording.
 * The endpoint must have been created by ms_audio_endpoint_new_recorder().
 * @param ep the endpoint
 * @return 0 if successful, -1 if the record wasn't started.
**/
MS2_PUBLIC int ms_audio_recorder_endpoint_stop(MSAudioEndpoint *ep);

/**
 * Destroy an audio endpoint.
 * @note Endpoints created by ms_audio_endpoint_get_from_stream() must be released by ms_audio_endpoint_release_from_stream().
**/
MS2_PUBLIC void ms_audio_endpoint_destroy(MSAudioEndpoint *ep);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

/**
 * @addtogroup mediastreamer2_video_conference
 * @{
 */

/**
 * Structure that holds audio conference parameters
**/
struct _MSVideoConferenceParams{
	int min_switch_interval;
	const char *codec_mime_type;
};

/**
 * Typedef to structure that holds conference parameters
**/
typedef struct _MSVideoConferenceParams MSVideoConferenceParams;

/**
 * The MSVideoConference is the object representing a video conference.
 *
 * First, the conference has to be created with ms_video_conference_new(), with parameters supplied.
 * Then, participants to the conference can be added with ms_video_conference_add_member().
 * Participants can be removed from the conference with ms_video_conference_remove_member().
 * The conference processing is performed in a new thread run by a MSTicker object, which is owned by the conference.
 * When all participants are removed, the MSVideoConference object can then be safely destroyed with ms_video_conference_destroy().
**/
typedef struct _MSVideoConference MSVideoConference;


/**
 * The MSVideoEndpoint represents a participant in the conference.
 * It can be constructed from an existing VideoStream object with
 * ms_video_endpoint_get_from_stream().
**/
typedef struct _MSVideoEndpoint MSVideoEndpoint;




#ifdef __cplusplus
extern "C" {
#endif

/**
 * Creates a conference.
 * @param params a MSVideoConferenceParams structure, containing conference parameters.
 * @returns a MSVideoConference object.
**/
MS2_PUBLIC MSVideoConference * ms_video_conference_new(MSFactory *factory, const MSVideoConferenceParams *params);

/**
 * Gets conference's current parameters.
 * @param obj the conference.
 * @returns a read-only pointer to the conference parameters.
**/
MS2_PUBLIC const MSVideoConferenceParams *ms_video_conference_get_params(MSVideoConference *obj);

/**
 * Adds a participant to the conference.
 * @param obj the conference
 * @param ep the participant, represented as a MSVideoEndpoint object
**/
MS2_PUBLIC void ms_video_conference_add_member(MSVideoConference *obj, MSVideoEndpoint *ep);

/**
 * Removes a participant from the conference.
 * @param obj the conference
 * @param ep the participant, represented as a MSVideoEndpoint object
**/
MS2_PUBLIC void ms_video_conference_remove_member(MSVideoConference *obj, MSVideoEndpoint *ep);


/**
 * Switch the focus of the video conf on a given member.
 * @param obj the conference
 * @param ep the participant, represented as a MSVideoEndpoint object
 */
MS2_PUBLIC void ms_video_conference_set_focus(MSVideoConference *obj, MSVideoEndpoint *ep);

/**
* Get the video placeholder member, as MSVideoEndpoint.
* @param obj the conference
* @return a MSVideoEndpoint object.
*/
MS2_PUBLIC MSVideoEndpoint *ms_video_conference_get_video_placeholder_member(const MSVideoConference *obj);

/**
 * Get the list of members, as MSVideoEndpoints.
 * @param obj the conference
 * @return a list of MSVideoEndpoint objects.
 */
MS2_PUBLIC const bctbx_list_t* ms_video_conference_get_members(const MSVideoConference *obj);

/**
 * Put an audio conference and a video conference in relationship.
 * The audio conference will monitor the active speaker, and notify the video conference.
 * @param obj the video conference
 * @param obj the audio conference
 */
MS2_PUBLIC void ms_video_conference_set_audio_conference(MSVideoConference *obj, MSAudioConference *audioconf);

/**
 * Returns the size (ie the number of participants) of a conference.
 * @param obj the conference
**/
MS2_PUBLIC int ms_video_conference_get_size(MSVideoConference *obj);

/**
 * Destroys a conference.
 * @param obj the conference
 * All participants must have been removed before destroying the conference.
**/
MS2_PUBLIC void ms_video_conference_destroy(MSVideoConference *obj);

/**
 * Creates an MSVideoEndpoint from an existing VideoStream.
 *
 * In order to create graphs for video processing of each participant, the VideoStream object is used, because
 * this object already handles all the processing for encoding, decoding, etc...
 *
 * The construction of the participants depends whether it is a remote participant, that is somebody in the network
 * sending and receiving video through RTP, or a local participant, that is somebody using the local camera to capture
 * and local screen to display video.
 *
 * To create a remote participant, first create and start a VideoStream for the participant with video_stream_new() and
 * video_stream_start() with NULL MSWebCam argument.
 * Then, create a MSVideoEndpoint representing this participant by calling ms_video_endpoint_get_from_stream() with
 * is_remote=TRUE.
 *
 * To create a local participant, first create and start a VideoStream with video_stream_new() and video_stream_start(), 
 * with the correct MSWebCam to use.
 * Arguments controlling RTP should be filled with placeholders value and will not be used for conferencing.
 * Then, create a MSVideoEndpoint representing this local participant by calling ms_video_endpoint_get_from_stream() 
 * with the video stream and is_remote=FALSE.<br>
 * For example:<br>
 * <PRE>
 * VideoStream *st=video_stream_new(65000,65001,FALSE);
 * video_stream_start(st, conf->local_dummy_profile,
 *				"127.0.0.1",
 *				65000,
 *				"127.0.0.1",
 *				65001,
 *				0,
 *				40,
 *				webcam
 *				);
 * MSVideoEndpoint *local_endpoint=ms_video_endpoint_get_from_stream(st,FALSE);
 * </PRE>
**/
MS2_PUBLIC MSVideoEndpoint * ms_video_endpoint_get_from_stream(VideoStream *st, bool_t is_remote);


/**
 * Associate a user pointer to the endpoint.
 * @param ep the endpoint
 * @param user_data the user data
 */
MS2_PUBLIC void ms_video_endpoint_set_user_data(MSVideoEndpoint *ep, void *user_data);

/**
 * Get the user pointer associated to the endpoint.
 * @param ep the endpoint
 * @return the user data
 */
MS2_PUBLIC void * ms_video_endpoint_get_user_data(const MSVideoEndpoint *ep);


/**
 * Destroys a MSVideoEndpoint that was created from a VideoStream with ms_video_endpoint_get_from_stream().
 * The VideoStream can then be destroyed if needed.
**/
MS2_PUBLIC void ms_video_endpoint_release_from_stream(MSVideoEndpoint *obj);


#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif

