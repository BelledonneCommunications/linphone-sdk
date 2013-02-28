/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011 Belledonne Communications SARL
Author: Simon MORLAT (simon.morlat@linphone.org)

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

/**
 * Structure that holds audio conference parameters
**/
struct _MSAudioConferenceParams{
	int samplerate; /**< Conference audio sampling rate in Hz: 8000, 16000 ...*/
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
 * @returns a MSAudioConference object.
**/
MS2_PUBLIC MSAudioConference * ms_audio_conference_new(const MSAudioConferenceParams *params);

/**
 * Gets conference's current parameters.
 * @param obj the conference.
 * @returns a read-only pointer to the conference parameters.
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
 * Mutes or unmutes a participant.
 * 
 * @param obj the conference
 * @param ep the participant, represented as a MSAudioEndpoint object
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
 * Destroys a MSAudioEndpoint that was created from an AudioStream with ms_audio_endpoint_get_from_stream().
 * The AudioStream can then be destroyed if needed.
**/
MS2_PUBLIC void ms_audio_endpoint_release_from_stream(MSAudioEndpoint *obj);

/**
 * Creates an audio endpoint (or virtual participant) to record the conference into a wav file.
 * @param path Path to the wav file to record.
**/
MS2_PUBLIC MSAudioEndpoint * ms_audio_endpoint_new_recorder(void);

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



#endif

