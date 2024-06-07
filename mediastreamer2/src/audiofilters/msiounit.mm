/*
 * auiosnd.c -I/O unit Media plugin for Linphone-
 *
 *
 * Copyright (C) 2009  Belledonne Comunications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVAudioSession.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>

#import "mediastreamer2/mssndcard.h"
#import "mediastreamer2/msfilter.h"
#import "mediastreamer2/msticker.h"
#import "mediastreamer2/msqueue.h"

#ifdef __cplusplus
extern "C"{
#endif
#include <bctoolbox/param_string.h>
#ifdef __cplusplus
}
#endif

static const int defaultSampleRate = 48000;
static const int flowControlInterval = 5000; // ms
static const int flowControlThreshold = 40; // ms

/*                          -------------------------
							| i                   o |
-- BUS 1 -- from mic -->	| n    REMOTE I/O     u | -- BUS 1 -- to app -->
							| p      AUDIO        t |
-- BUS 0 -- from app -->	| u       UNIT        p | -- BUS 0 -- to speaker -->
							| t                   u |
							|                     t |
							-------------------------
 */

static AudioUnitElement inputBus = 1;
static AudioUnitElement outputBus = 0;

static const char *audio_unit_format_error (OSStatus error) {
	switch (error) {
		case kAudioUnitErr_InvalidProperty: return "kAudioUnitErr_InvalidProperty";
		case kAudioUnitErr_InvalidParameter: return "kAudioUnitErr_InvalidParameter";
		case kAudioUnitErr_InvalidElement: return "kAudioUnitErr_InvalidElement";
		case kAudioUnitErr_NoConnection: return "kAudioUnitErr_NoConnection";
		case kAudioUnitErr_FailedInitialization: return "kAudioUnitErr_FailedInitialization";
		case kAudioUnitErr_TooManyFramesToProcess: return "kAudioUnitErr_TooManyFramesToProcess";
		case kAudioUnitErr_InvalidFile: return "kAudioUnitErr_InvalidFile";
		case kAudioUnitErr_UnknownFileType: return "kAudioUnitErr_UnknownFileType";
		case kAudioUnitErr_FileNotSpecified: return "kAudioUnitErr_FileNotSpecified";
		case kAudioUnitErr_FormatNotSupported: return "kAudioUnitErr_FormatNotSupported";
		case kAudioUnitErr_Uninitialized: return "kAudioUnitErr_Uninitialized";
		case kAudioUnitErr_InvalidScope: return "kAudioUnitErr_InvalidScope";
		case kAudioUnitErr_PropertyNotWritable: return "kAudioUnitErr_PropertyNotWritable";
		case kAudioUnitErr_CannotDoInCurrentContext: return "kAudioUnitErr_CannotDoInCurrentContext";
		case kAudioUnitErr_InvalidPropertyValue: return "kAudioUnitErr_InvalidPropertyValue";
		case kAudioUnitErr_PropertyNotInUse: return "kAudioUnitErr_PropertyNotInUse";
		case kAudioUnitErr_Initialized: return "kAudioUnitErr_Initialized";
		case kAudioUnitErr_InvalidOfflineRender: return "kAudioUnitErr_InvalidOfflineRender";
		case kAudioUnitErr_Unauthorized: return "kAudioUnitErr_Unauthorized";
		default: {
			ms_error ("Cannot start audioUnit because [%c%c%c%c]"
						  ,((char*)&error)[3]
						  ,((char*)&error)[2]
						  ,((char*)&error)[1]
						  ,((char*)&error)[0]);
			return "unknown error";
		}
	}

}

#define check_au_session_result(au,method) \
if (au!=AVAudioSessionErrorInsufficientPriority && au!=0) ms_error("AudioSession error for %s: ret=%i (%s:%d)",method, au, __FILE__, __LINE__)

#define check_au_unit_result(au,method) \
if (au!=0) ms_error("AudioUnit error for %s: ret=%s (%li) (%s:%d)",method, audio_unit_format_error(au), (long)au, __FILE__, __LINE__)

#define check_session_call(call)   do { OSStatus res = (call); check_au_session_result(res, #call); } while(0)
#define check_audiounit_call(call) do { OSStatus res = (call); check_au_unit_result(res, #call); } while(0)

static const char * SCM_PARAM_FAST = "FAST";
static const char * SCM_PARAM_NOVOICEPROC = "NOVOICEPROC";
static const char * SCM_PARAM_TESTER = "TESTER";
static const char * SCM_PARAM_RINGER = "RINGER";
static const char * MS_SND_CARD_SPEAKER_PORT_NAME = "Speaker";

static MSFilter *ms_au_read_new(MSSndCard *card);
static MSFilter *ms_au_write_new(MSSndCard *card);

typedef struct au_filter_read_data au_filter_read_data_t;
typedef struct au_filter_write_data au_filter_write_data_t;

typedef enum _MSAudioUnitState{
	MSAudioUnitNotCreated,
	MSAudioUnitCreated,
	MSAudioUnitConfigured,
	MSAudioUnitStarted
} MSAudioUnitState;

@interface AudioUnitHolder : NSObject

@property AudioUnit	audio_unit;
@property MSAudioUnitState audio_unit_state;
@property unsigned int	rate;
@property unsigned int	bits;
@property unsigned int	nchannels;
@property uint64_t last_failed_iounit_start_time;
@property MSFilter* read_filter;
@property MSFilter* write_filter;
@property MSSndCard* ms_snd_card;
@property bool_t audio_session_configured;
@property bool_t read_started;
@property bool_t write_started;
@property bool_t will_be_used;
@property bool_t audio_session_activated;
@property bool_t callkit_enabled;
@property bool_t mic_enabled;
@property bool_t zombified;
@property bool_t stalled;
@property bool_t devices_changed_since_last_reload;
@property bool_t interacted_with_bluetooth_since_last_devices_reload;
@property std::string removedDevice;

+(AudioUnitHolder *)sharedInstance;
- (id)init;
-(void)create_audio_unit;
-(void)configure_audio_unit;
-(bool_t)start_audio_unit:(uint64_t) time;
-(void)stop_audio_unit_with_param:(bool_t) isConfigured;
-(void)stop_audio_unit;
-(void)destroy_audio_unit: (bool_t) with_snd_card;
-(void)mutex_lock;
-(void)mutex_unlock;

-(void) check_audio_unit_is_up;
-(void) configure_audio_session;
-(void) onAudioRouteChange: (NSNotification *) notif;
@end

typedef struct au_filter_base {
	int muted;
} au_filter_base_t;

struct au_filter_read_data{
	au_filter_base_t base;
	ms_mutex_t	mutex;
	queue_t		rq;
	AudioTimeStamp readTimeStamp;
	unsigned int n_lost_frame;
	MSTickerSynchronizer *ticker_synchronizer;
	uint64_t read_samples;
	uint64_t read_samples_last_activity_check;
};

struct au_filter_write_data{
	au_filter_base_t base;
	AudioTimeStamp writeTimeStamp;
	ms_mutex_t mutex;
	MSFlowControlledBufferizer *bufferizer;
	unsigned int n_lost_frame;
};

static void update_audio_unit_holder_ms_snd_card(MSSndCard * newcard) {
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	MSSndCard * oldCard = [au_holder ms_snd_card];

	ms_snd_card_ref(newcard);
	[au_holder mutex_lock];
	[au_holder setMs_snd_card:newcard];
	[au_holder mutex_unlock];
	if (oldCard != NULL) {
		ms_snd_card_unref(oldCard);
	}
	ms_message("au_update_ms_snd_card");
}

struct AUCard{
	static std::string portToName(const char *portName){
		return "[" + std::string(portName) + "]";
	}
	static AUCard * get(MSSndCard *card){
		return static_cast<AUCard*>(card->data);
	}
	std::string mPortName;
};

/*
 mediastreamer2 function
 */

static void au_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent) {}

static int au_get_level(MSSndCard *card, MSSndCardMixerElem e) {
	return 0;
}

static void au_set_source(MSSndCard *card, MSSndCardCapture source) {}

static OSStatus au_render_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
							  );

static OSStatus au_read_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
);

static OSStatus au_write_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
							 );

static MSSndCardDeviceType deduceDeviceTypeFromAudioPortType(AVAudioSessionPort port)
{
	if ([port isEqualToString:(AVAudioSessionPortBuiltInMic)])	{
		return MS_SND_CARD_DEVICE_TYPE_MICROPHONE;
	} else if ([port isEqualToString:(AVAudioSessionPortBluetoothHFP)])	{
		return MS_SND_CARD_DEVICE_TYPE_BLUETOOTH;
	} else if (([port isEqualToString:(AVAudioSessionPortBluetoothA2DP)])) {
		return MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP;
	} else if ([port isEqualToString:(AVAudioSessionPortHeadsetMic)])	{
		return MS_SND_CARD_DEVICE_TYPE_HEADPHONES;
	}

	return MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
}

static bool apply_current_route_to_default_snd_card(MSSndCard *card){
	bool changed = false;
	if (!(ms_snd_card_get_capabilities(card) & MS_SND_CARD_CAP_FOLLOWS_SYSTEM_POLICY)) return false;
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];
	AVAudioSessionRouteDescription *currentRoute = [audioSession currentRoute];
	
	bool currentOutputIsSpeaker = (currentRoute.outputs.count > 0 && strcmp(currentRoute.outputs[0].portType.UTF8String, AVAudioSessionPortBuiltInSpeaker.UTF8String) == 0);
	if (currentOutputIsSpeaker){
		changed = card->device_type != MS_SND_CARD_DEVICE_TYPE_SPEAKER;
		card->device_type = MS_SND_CARD_DEVICE_TYPE_SPEAKER;
		return changed;
	}
	if (currentRoute.inputs.count == 0) return false;
	MSSndCardDeviceType dt = deduceDeviceTypeFromAudioPortType(currentRoute.inputs[0].portType);
	changed = dt != card->device_type;
	card->device_type = dt;
	AUCard::get(card)->mPortName = [currentRoute.inputs[0].portName UTF8String];
	return changed;
}

static int apply_sound_card_to_audio_session(MSSndCard * newCard) {
	ms_message("apply_sound_card_to_audio_session()");
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];
	AVAudioSessionRouteDescription *currentRoute = [audioSession currentRoute];
	NSError *err=nil;
	if (newCard->device_type == MS_SND_CARD_DEVICE_TYPE_SPEAKER)
	{
		bool_t currentOutputIsSpeaker = (currentRoute.outputs.count > 0 && strcmp(currentRoute.outputs[0].portType.UTF8String, AVAudioSessionPortBuiltInSpeaker.UTF8String) == 0);
		if (!currentOutputIsSpeaker) {
			// If we're switching to speaker and the route output isn't the speaker already
			ms_message("set_audio_unit_sound_card(): change AVAudioSession output audio to speaker");
			[audioSession overrideOutputAudioPort:AVAudioSessionPortOverrideSpeaker error:&err];
		}
	}
	else {
		// As AudioSession do not allow a way to nicely change the output port except with the override to Speaker,
		// we assume that input ports also come with a playback port (bluetooth earpiece, headset...) and change the input port.
		NSString *newPortName = [NSString stringWithUTF8String: AUCard::get(newCard)->mPortName.c_str()];
		NSArray *inputs = [audioSession availableInputs];
		for (AVAudioSessionPortDescription *input in inputs) {
			if ([input.portName isEqualToString:newPortName ]) {
				ms_message("apply_sound_card_to_audio_session(): change AVAudioSession preferred input to %s.", [newPortName UTF8String]);
				[audioSession setPreferredInput:input error:&err];
				break;
			}
		}
	}
	return 0;
}


/**
 * AudioUnit helper functions, to associate the AudioUnit with the MSSndCard object used by mediastreamer2.
 */

@implementation AudioUnitHolder

ms_mutex_t mutex;

+ (AudioUnitHolder *)sharedInstance {
	static AudioUnitHolder *sharedInstance = nil;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		sharedInstance = [[self alloc] init];
	});
	return sharedInstance;
}

- (id)init {
	if (self = [super init]) {
		ms_debug("au_init");
		_bits=16;
		_rate=defaultSampleRate;
		_nchannels=1;
		_ms_snd_card=NULL;
		_write_filter=NULL;
		_read_filter=NULL;
		_will_be_used = FALSE;
		ms_mutex_init(&mutex,NULL);
		_mic_enabled = TRUE;
		_zombified = FALSE;
		_stalled = FALSE;
		_interacted_with_bluetooth_since_last_devices_reload = FALSE;
		_devices_changed_since_last_reload=FALSE;
		[NSNotificationCenter.defaultCenter addObserver:self
											   selector:@selector(onAudioRouteChange:)
												   name:AVAudioSessionRouteChangeNotification
												 object:nil];

	}
	return self;
}

-(void)create_audio_unit {
	AudioComponentDescription au_description;
	AudioComponent foundComponent;

	if (_audio_unit != NULL) return;
	if (_ms_snd_card == NULL) {
		ms_error("create_audio_unit(): not created because no associated ms_snd_card was found");
		return;
	}
	bool_t noVoiceProc = bctbx_param_string_get_bool_value(_ms_snd_card->sndcardmanager->paramString, SCM_PARAM_NOVOICEPROC) || _ms_snd_card->streamType == MS_SND_CARD_STREAM_MEDIA;
	OSType subtype = noVoiceProc ? kAudioUnitSubType_RemoteIO : kAudioUnitSubType_VoiceProcessingIO;

	au_description.componentType          = kAudioUnitType_Output;
	au_description.componentSubType       = subtype;
	au_description.componentManufacturer  = kAudioUnitManufacturer_Apple;
	au_description.componentFlags         = 0;
	au_description.componentFlagsMask     = 0;

	foundComponent = AudioComponentFindNext (NULL,&au_description);

	check_audiounit_call( AudioComponentInstanceNew(foundComponent, &_audio_unit) );

	//Always configure readcb
	AURenderCallbackStruct renderCallbackStruct;
	renderCallbackStruct.inputProc       = au_read_cb;
	//renderCallbackStruct.inputProcRefCon = au_holder;
	check_audiounit_call(AudioUnitSetProperty (
											   _audio_unit,
											   kAudioOutputUnitProperty_SetInputCallback,
											   kAudioUnitScope_Input,
											   outputBus,
											   &renderCallbackStruct,
											   sizeof (renderCallbackStruct)
											   ));

	if (_audio_unit) {
		ms_message("AudioUnit created with type %s.", subtype==kAudioUnitSubType_RemoteIO ? "kAudioUnitSubType_RemoteIO" : "kAudioUnitSubType_VoiceProcessingIO" );
		_audio_unit_state = MSAudioUnitCreated;
		_zombified = FALSE;
		_stalled = FALSE;
	}
	return;
}

-(void)configure_audio_unit {
	OSStatus auresult;
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];

	if (_audio_unit_state != MSAudioUnitCreated){
		ms_error("configure_audio_unit(): not created, in state %i", _audio_unit_state);
		return;
	}
	uint64_t time_start, time_end;

	time_start = bctbx_get_cur_time_ms();
	ms_message("configure_audio_unit() now called.");
	
	/*Apply the selected route, unless the sndcard is the one that let the system decides. */
	if (_ms_snd_card){
		if (ms_snd_card_get_capabilities(_ms_snd_card) & MS_SND_CARD_CAP_FOLLOWS_SYSTEM_POLICY){
			apply_current_route_to_default_snd_card(_ms_snd_card);
		}else{
			apply_sound_card_to_audio_session(_ms_snd_card);
		}
	}

	AudioStreamBasicDescription audioFormat;
	/*card sampling rate is fixed at that time*/
	audioFormat.mSampleRate			= _rate;
	audioFormat.mFormatID			= kAudioFormatLinearPCM;
	audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	audioFormat.mFramesPerPacket	= 1;
	audioFormat.mChannelsPerFrame	= _nchannels;
	audioFormat.mBitsPerChannel		= _bits;
	audioFormat.mBytesPerPacket		= _bits / 8;
	audioFormat.mBytesPerFrame		= _nchannels * _bits / 8;

	UInt32 doNotSetProperty    = 0;
	UInt32 doSetProperty    = 1;
	
	bool_t recording = audioSession.category == AVAudioSessionCategoryPlayAndRecord;
	
	if (!recording) ms_message("configure_audio_unit(): configured for playback only.");

	//enable speaker output
	auresult =AudioUnitSetProperty (
									_audio_unit,
									kAudioOutputUnitProperty_EnableIO,
									kAudioUnitScope_Output ,
									outputBus,
									&doSetProperty,
									sizeof (doSetProperty)
									);
	check_au_unit_result(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Output");

	/*enable mic for scheduling render call back, why ?*/
	auresult=AudioUnitSetProperty (/*enable mic input*/
								   _audio_unit,
								   kAudioOutputUnitProperty_EnableIO,
								   kAudioUnitScope_Input ,
								   inputBus,
								   _mic_enabled && recording ? &doSetProperty : &doNotSetProperty,
								   sizeof (_mic_enabled ? doSetProperty : doNotSetProperty)
								   );

	check_au_unit_result(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Input");
	auresult=AudioUnitSetProperty (
								   _audio_unit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Output,
								   inputBus,
								   &audioFormat,
								   sizeof (audioFormat)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output");
	/*end of: enable mic for scheduling render call back, why ?*/

	//setup stream format
	auresult=AudioUnitSetProperty (
								   _audio_unit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Input,
								   outputBus,
								   &audioFormat,
								   sizeof (audioFormat)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input");

	//disable unit buffer allocation
	auresult=AudioUnitSetProperty (
								   _audio_unit,
								   kAudioUnitProperty_ShouldAllocateBuffer,
								   kAudioUnitScope_Output,
								   outputBus,
								   &doNotSetProperty,
								   sizeof (doNotSetProperty)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_ShouldAllocateBuffer,kAudioUnitScope_Output");
	AURenderCallbackStruct renderCallbackStruct;
	renderCallbackStruct.inputProc       = au_write_cb;
	//renderCallbackStruct.inputProcRefCon = au_holder;

	auresult=AudioUnitSetProperty (
								   _audio_unit,
								   kAudioUnitProperty_SetRenderCallback,
								   kAudioUnitScope_Input,
								   outputBus,
								   &renderCallbackStruct,
								   sizeof (renderCallbackStruct)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input");
	time_end = bctbx_get_cur_time_ms();
	ms_message("configure_audio_unit() took %i ms.", (int)(time_end - time_start));
	_audio_unit_state=MSAudioUnitConfigured;
}

-(bool_t)start_audio_unit: (uint64_t) time {

	if (_audio_unit_state != MSAudioUnitConfigured){
		ms_error("start_audio_unit(): state is %i", _audio_unit_state);
		return FALSE;
	}
	uint64_t time_start, time_end;
	time_start = bctbx_get_cur_time_ms();
	ms_message("start_audio_unit(): about to start audio unit.");
	check_audiounit_call(AudioUnitInitialize(_audio_unit));

	Float64 delay;
	UInt32 delaySize = sizeof(delay);
	check_audiounit_call(AudioUnitGetProperty(_audio_unit
									,kAudioUnitProperty_Latency
									, kAudioUnitScope_Global
									, 0
									, &delay
									, &delaySize));

	UInt32 quality;
	UInt32 qualitySize = sizeof(quality);
	check_audiounit_call(AudioUnitGetProperty(_audio_unit
									,kAudioUnitProperty_RenderQuality
									, kAudioUnitScope_Global
									, 0
									, &quality
									, &qualitySize));
	ms_message("I/O unit latency [%f], quality [%u]",delay,(unsigned)quality);
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];
	Float32 hwoutputlatency = audioSession.outputLatency;

	Float32 hwinputlatency = audioSession.inputLatency;

	Float32 hwiobuf = audioSession.IOBufferDuration;

	Float64 hwsamplerate = audioSession.sampleRate;

	OSStatus auresult;
	check_audiounit_call( (auresult = AudioOutputUnitStart(_audio_unit)) );
	if (auresult == 0){
		_audio_unit_state = MSAudioUnitStarted;
	}
	if (_audio_unit_state != MSAudioUnitStarted) {
		ms_message("AudioUnit could not be started, current hw output latency [%f] input [%f] iobuf[%f] hw sample rate [%f]",hwoutputlatency,hwinputlatency,hwiobuf,hwsamplerate);
		_last_failed_iounit_start_time = time;
	} else {
		ms_message("AudioUnit started, current hw output latency [%f] input [%f] iobuf[%f] hw sample rate [%f]",hwoutputlatency,hwinputlatency,hwiobuf,hwsamplerate);
		_last_failed_iounit_start_time = 0;
	}
	time_end = bctbx_get_cur_time_ms();
	ms_message("start_audio_unit() took %i ms.", (int)(time_end - time_start));

	return (_audio_unit_state == MSAudioUnitStarted);
}

-(void)stop_audio_unit_with_param: (bool_t) isConfigured {
	if (_audio_unit_state == MSAudioUnitStarted || _audio_unit_state == MSAudioUnitConfigured) {
		check_audiounit_call( AudioOutputUnitStop(_audio_unit) );
		ms_message("AudioUnit stopped");
		_audio_session_configured = isConfigured;
		check_audiounit_call( AudioUnitUninitialize(_audio_unit) );
		_audio_unit_state = MSAudioUnitCreated;
	}
}

-(void)stop_audio_unit {
	[self stop_audio_unit_with_param:FALSE];
}

-(void)recreate_audio_unit{
	[self stop_audio_unit_with_param:FALSE];
	if (_audio_unit) {
		AudioComponentInstanceDispose (_audio_unit);
		_audio_unit = NULL;
	}
	[self create_audio_unit];
}

-(void)destroy_audio_unit: (bool_t) with_snd_card{
	[self stop_audio_unit];

	bool isFast = (_ms_snd_card && bctbx_param_string_get_bool_value(_ms_snd_card->sndcardmanager->paramString, SCM_PARAM_FAST));
	if (with_snd_card && _ms_snd_card) {
		MSSndCard * cardbuffer = _ms_snd_card;
		[self mutex_lock];
		_ms_snd_card = NULL;
		[self mutex_unlock];
		ms_snd_card_unref(cardbuffer);
		ms_message("au_destroy_audio_unit set holder card to NULL");
	}

	if (_audio_unit) {
		AudioComponentInstanceDispose (_audio_unit);
		_audio_unit = NULL;

		if (!_callkit_enabled && !isFast)  {
			NSError *err = nil;
			[[AVAudioSession sharedInstance] setActive:FALSE withOptions:AVAudioSessionSetActiveOptionNotifyOthersOnDeactivation error:&err];
			if(err) ms_error("Unable to activate audio session because : %s", [err localizedDescription].UTF8String);
			err = nil;
		}
		ms_message("AudioUnit destroyed");
		_audio_unit_state = MSAudioUnitNotCreated;
		if (_zombified){
			_zombified = FALSE;
		}
	}
}

-(void)mutex_lock {
	ms_mutex_lock(&mutex);
}
-(void)mutex_unlock {
	ms_mutex_unlock(&mutex);
}

-(void) configure_audio_session {
	NSError *err = nil;
	//UInt32 audioCategorySize=sizeof(audioCategory);
	AVAudioSession *audioSession = [AVAudioSession sharedInstance];

	if (_audio_unit_state == MSAudioUnitStarted){
		ms_message("configure_audio_session(): AudioUnit is already started, skipping this process.");
		return;
	}

	if (_ms_snd_card && _ms_snd_card->sndcardmanager && !bctbx_param_string_get_bool_value(_ms_snd_card->sndcardmanager->paramString, SCM_PARAM_FAST) ) {
		/*check that category wasn't changed*/
		NSString *currentCategory = audioSession.category;
		NSString *newCategory = AVAudioSessionCategoryPlayAndRecord;
		AVAudioSessionMode currentMode = audioSession.mode;
		AVAudioSessionMode newMode = AVAudioSessionModeVoiceChat;
		AVAudioSessionCategoryOptions options = AVAudioSessionCategoryOptionAllowBluetooth| AVAudioSessionCategoryOptionAllowBluetoothA2DP;

		if (_ms_snd_card->streamType == MS_SND_CARD_STREAM_MEDIA){
			ms_message("Configuring audio session for playback.");
			newCategory = AVAudioSessionCategoryPlayback;
			newMode = AVAudioSessionModeDefault;
			options = 0;
		}else if (bctbx_param_string_get_bool_value(_ms_snd_card->sndcardmanager->paramString, SCM_PARAM_RINGER) ){
			ms_message("Configuring audio session for ambiant sound.");
			newCategory = AVAudioSessionCategoryAmbient;
			newMode = AVAudioSessionModeDefault;
			options = 0;
		}

		if (currentCategory != newCategory || currentMode != AVAudioSessionModeVoiceChat){
			_audio_session_configured = FALSE;
		}

		if (!_audio_session_configured) {
			uint64_t time_start, time_end;
			bool_t reactivate_audio_session = FALSE;

			time_start = bctbx_get_cur_time_ms();
			if (_audio_session_activated && _callkit_enabled){
				/* AudioSession is not yet configured, but has already been activated by callkit. */
				[audioSession setActive:FALSE error:&err];
				reactivate_audio_session = TRUE;
			}
				
			[audioSession setCategory:newCategory
									withOptions: options
									error: &err];
			if (err){
				ms_error("Unable to change audio session category because : %s", [err localizedDescription].UTF8String);
				err = nil;
			}
			[audioSession setMode:newMode error:&err];
			if(err){
				ms_error("Unable to change audio session mode because : %s", [err localizedDescription].UTF8String);
				err = nil;
			}
			double sampleRate = defaultSampleRate;
			[audioSession setPreferredSampleRate:sampleRate error:&err];
			if (err) {
				ms_error("Unable to change preferred sample rate because : %s", [err localizedDescription].UTF8String);
				err = nil;
			}
			if (!_callkit_enabled || reactivate_audio_session){
				if (reactivate_audio_session) ms_message("Configuring audio session now reactivated.");
				[audioSession setActive:TRUE error:&err];
				if(err){
					ms_error("Unable to activate audio session because : %s", [err localizedDescription].UTF8String);
					err = nil;
				} else {
					if (_ms_snd_card && _ms_snd_card->device_type != MS_SND_CARD_DEVICE_TYPE_MICROPHONE) {
						// Activating the audio session, by default, will change the audio route to iphone microphone/receiver
						// To avoid this, we redirect the sound to the current snd card
						apply_sound_card_to_audio_session(_ms_snd_card);
					}
				}
			}else ms_message("Not activating the AVAudioSession because it is CallKit's job.");

			time_end = bctbx_get_cur_time_ms();
			ms_message("MSAURead/MSAUWrite: configureAudioSession() took %i ms.", (int)(time_end - time_start));
			_audio_session_configured = TRUE;
		} else {
			ms_message("Audio session already correctly configured.");
		}

	} else {
		ms_message("Fast iounit mode, audio session configuration must be done at application level.");
	}
	/*
		Now that the AudioSession is configured and only if activation was done, take the audioSession's sampleRate.
		Otherwise the value is not valid.
	*/
	if (!_callkit_enabled || _audio_session_activated){
		_rate = (int)[audioSession sampleRate];
		ms_message("MSAURead/MSAUWrite: AVAudioSession is configured at sample rate %i.", _rate);
	}
}


-(void) check_audio_unit_is_up {
	[self configure_audio_session];
	if (_audio_unit_state == MSAudioUnitNotCreated){
		[self create_audio_unit];
	}
	if (_audio_unit_state == MSAudioUnitCreated && _audio_session_activated){
		[self configure_audio_unit];
	}
	if (_audio_unit_state == MSAudioUnitConfigured){
		[self start_audio_unit:0];
	}
	if (_audio_unit_state == MSAudioUnitStarted){
		ms_message("check_audio_unit_is_up(): audio unit is started.");
	}
}


- (void)onAudioRouteChange: (NSNotification *) notif {
	ms_message("[IOS Audio Route Change] msiounit audio route change callback");
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];

	AVAudioSession * audioSession = [AVAudioSession sharedInstance];
	NSDictionary * userInfo = [notif userInfo];
	NSInteger changeReason = [[userInfo valueForKey:AVAudioSessionRouteChangeReasonKey] integerValue];
	AVAudioSessionRouteDescription *previousRoute = [userInfo valueForKey:AVAudioSessionRouteChangePreviousRouteKey];
	AVAudioSessionRouteDescription *currentRoute = [audioSession currentRoute];

	std::string previousInputPort("No input");
	std::string currentInputPort("No input");
	std::string previousOutputPort("No output");
	std::string currentOutputPort("No output");
	bool currentOutputIsSpeaker = false, currentOutputIsReceiver = false;

	if (previousRoute.inputs.count > 0)
		previousInputPort = std::string([previousRoute.inputs[0].portName UTF8String]);
	if (previousRoute.outputs.count > 0) {
		previousOutputPort = std::string([previousRoute.outputs[0].portName UTF8String]);
		MSSndCardDeviceType portType = deduceDeviceTypeFromAudioPortType(previousRoute.outputs[0].portType);
		if (portType == MS_SND_CARD_DEVICE_TYPE_BLUETOOTH || portType == MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP) {
			[au_holder setInteracted_with_bluetooth_since_last_devices_reload:TRUE];
		}
	}
	if (currentRoute.inputs.count > 0)
		currentInputPort = std::string([currentRoute.inputs[0].portName UTF8String]);
	if (currentRoute.outputs.count > 0) {
		currentOutputPort = std::string([currentRoute.outputs[0].portName UTF8String]);
		currentOutputIsSpeaker = (strcmp(currentRoute.outputs[0].portType.UTF8String, AVAudioSessionPortBuiltInSpeaker.UTF8String) == 0);
		currentOutputIsReceiver = (strcmp(currentRoute.outputs[0].portType.UTF8String, AVAudioSessionPortBuiltInReceiver.UTF8String) == 0);

		MSSndCardDeviceType portType = deduceDeviceTypeFromAudioPortType(currentRoute.outputs[0].portType);
		if (portType == MS_SND_CARD_DEVICE_TYPE_BLUETOOTH || portType == MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP) {
			[au_holder setInteracted_with_bluetooth_since_last_devices_reload:TRUE];
		}
	}

	ms_message("[IOS Audio Route Change] Previous audio route: input=%s, output=%s, New audio route: input=%s, output=%s"
			   , previousInputPort.c_str(), previousOutputPort.c_str()
			   , currentInputPort.c_str(), currentOutputPort.c_str());


	switch (changeReason)
	{
		case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
			ms_message("[IOS Audio Route Change] new device %s available", currentOutputPort.c_str());
			[au_holder setRemovedDevice:""];
			[au_holder setDevices_changed_since_last_reload:TRUE];
			break;
		case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
			ms_message("[IOS Audio Route Change] old device %s unavailable", previousOutputPort.c_str());
			[au_holder setRemovedDevice:previousOutputPort];
			[au_holder setDevices_changed_since_last_reload:TRUE];
			break;
		default: {}
	}
	if (![au_holder ms_snd_card] || ![au_holder read_filter]) {
		ms_message("[IOS Audio Route Change]  Audio unit not initialized, ignore route change");
		return;
	}

	MSSndCard * previousCard = [au_holder ms_snd_card];
	MSAudioRouteChangedEvent ev;
	memset(&ev, 0, sizeof (ev));
	ev.need_update_device_list = [au_holder interacted_with_bluetooth_since_last_devices_reload];
	ev.has_new_input = false;
	ev.has_new_output = false;

	if (previousInputPort != currentInputPort && AUCard::get(previousCard)->mPortName == currentInputPort) {
		ev.has_new_input = true;
		strncpy(ev.new_input, AUCard::portToName(currentInputPort.c_str()).c_str(), sizeof(ev.new_input)-1);
	}

	std::string newOutput = AUCard::portToName(currentOutputPort.c_str());
	if (previousOutputPort != currentOutputPort) {
		if (currentOutputIsSpeaker) {
			newOutput = AUCard::portToName(MS_SND_CARD_SPEAKER_PORT_NAME);
		}
		else {
			MSSndCardManager * sndCardManager = ms_factory_get_snd_card_manager(ms_snd_card_get_factory(previousCard));
			bctbx_list_t *cardIt;
			for (cardIt=sndCardManager->cards; cardIt!=NULL; cardIt=cardIt->next) {
				MSSndCard *card = (MSSndCard*)cardIt->data;
				// Special case : the sndcard matching the IPhoneMicrophone and the IPhoneReceiver is the same.
				// They are built using the AVAudioSession.availableInputs function, which is why the name will not match
				if ( currentOutputIsReceiver && card->device_type == MS_SND_CARD_DEVICE_TYPE_MICROPHONE ) {
					newOutput = card->name;
					break;
				}
			}
		}
	}

	if (strcmp(previousCard->name, newOutput.c_str()) != 0) {
		ev.has_new_output = true;
		strncpy(ev.new_output, newOutput.c_str(), sizeof(ev.new_output)-1);
	}

	switch (changeReason)
	{
		case AVAudioSessionRouteChangeReasonNewDeviceAvailable:
		case AVAudioSessionRouteChangeReasonOldDeviceUnavailable:
		case AVAudioSessionRouteChangeReasonCategoryChange:
		{
			// We need to reload for these 3 category, because the list of sound cards available may not be up to date
			// For example, bluetooth devices would possibly not be detected before a call Start, as the AudioSession may be in a category other than AVAudioSessionCategoryPlayAndRecord
			ev.need_update_device_list = true;
		}
		default: {}
	}
	if (au_holder.ms_snd_card){
		/* update the default card current device type */
		if (ms_snd_card_get_capabilities(au_holder.ms_snd_card) & MS_SND_CARD_CAP_FOLLOWS_SYSTEM_POLICY){
			if (apply_current_route_to_default_snd_card(au_holder.ms_snd_card)){
				ev.need_update_device_list = TRUE;
			}
		}
	}
	if ( !(ev.has_new_input || ev.has_new_output || ev.need_update_device_list) ) {
		ms_message("[IOS Audio Route Change] Audio unit already matches the new audio route, skipping");
		return;
	}
	
	ms_filter_notify([au_holder read_filter], MS_AUDIO_ROUTE_CHANGED, &ev);
}
@end

/* the interruption listener is not reliable, it can be overriden by other parts of the application */
/* as a result, we do nothing with it*/
static void au_interruption_listener (void *inClientData, UInt32 inInterruptionState) {}

static void au_init(MSSndCard *card){
	ms_debug("au_init");
	card->preferred_sample_rate=defaultSampleRate;
	card->capabilities |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER|MS_SND_CARD_CAP_IS_SLOW;
	card->data = new AUCard();
}

static void au_uninit(MSSndCard *card){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	if ([au_holder ms_snd_card] == card) {
		[au_holder destroy_audio_unit: TRUE];
	}
	delete (AUCard*)card->data;
}
// If the both read&write filters have been destroyed, we're not in test mode, and will_be_used == FALSE, then stop&destroy the audio unit
static void destroy_audio_unit_if_not_needed_anymore(){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	if ([au_holder audio_unit_state] == MSAudioUnitNotCreated || [au_holder read_filter] || [au_holder write_filter])
		return;

	if ( ([au_holder ms_snd_card] != NULL && bctbx_param_string_get_bool_value([au_holder ms_snd_card]->sndcardmanager->paramString, SCM_PARAM_TESTER)) || ![au_holder will_be_used] ) {
		if (ms_snd_card_get_capabilities(au_holder.ms_snd_card) & MS_SND_CARD_CAP_FOLLOWS_SYSTEM_POLICY){
			// Once the audio unit is destroyed, the audio route is almost unknown.*/
			au_holder.ms_snd_card->device_type = MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
		}
		[au_holder destroy_audio_unit: TRUE];
	}
}

static void au_usage_hint(MSSndCard *card, bool_t used){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	[au_holder setWill_be_used:used];
	destroy_audio_unit_if_not_needed_anymore();
}

static void au_detect(MSSndCardManager *m);
static MSSndCard *au_duplicate(MSSndCard *obj);
static bool_t au_reload_requested(MSSndCardManager *m);

static void handle_sample_rate_change(int force){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	int rate = (int)[[AVAudioSession sharedInstance] sampleRate];
	if (!force && rate == (int)[au_holder rate]) {
		return;
	}
	if (!force) ms_message("MSAURead/MSAUWrite: AVAudioSession's sample rate has changed from %i to %i.", [au_holder rate], rate);
	else ms_message("Updating sample rate since audio graph may be running.");
	[au_holder setRate:rate];

	if ([au_holder audio_unit_state] == MSAudioUnitConfigured || [au_holder audio_unit_state] == MSAudioUnitStarted){
		[au_holder recreate_audio_unit];
		[au_holder configure_audio_unit];
		[au_holder start_audio_unit:0];
	}

	if ([au_holder write_filter]) {
		au_filter_write_data_t *ft=(au_filter_write_data_t*)[au_holder write_filter]->data;
		ms_flow_controlled_bufferizer_set_samplerate(ft->bufferizer, rate);
		ms_filter_notify_no_arg([au_holder write_filter], MS_FILTER_OUTPUT_FMT_CHANGED);
	}
	if ([au_holder read_filter]) {
		ms_filter_notify_no_arg([au_holder read_filter], MS_FILTER_OUTPUT_FMT_CHANGED);
	}

}

static void au_audio_route_changed(MSSndCard *obj) {
	handle_sample_rate_change(FALSE);
}

static void au_audio_session_activated(MSSndCard *obj, bool_t activated) {
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	bool_t need_audio_session_reconfiguration = FALSE;
	ms_message("AVAudioSession activated: %i", (int)activated);
	
	if (!au_holder.callkit_enabled){
		ms_error("AudioUnit: AudioSession activation notified but not in Callkit mode: this is an application mistake.");
		return;
	}

	if (au_holder.audio_session_activated && activated && [au_holder audio_unit_state] != MSAudioUnitNotCreated){
		ms_warning("Callkit notifies that AVAudioSession is activated while it was supposed to be already activated. It means that a device disconnection happened.");
		/* The audio unit has to be re-created. Not documented anywhere but shown by direct experience. */
		[au_holder recreate_audio_unit];
	}else if (!au_holder.audio_session_activated && !activated){
		ms_warning("Audio session notified of deactivation twice... Loosy iOS developers.");
		return;
	}else if (au_holder.zombified){
		ms_warning("AudioUnit was marked as zombified. Recreating it.");
		[au_holder destroy_audio_unit:FALSE];
		need_audio_session_reconfiguration = TRUE;
	}
	[au_holder setAudio_session_activated:activated];

	if (activated){
		if (need_audio_session_reconfiguration) {
			[au_holder configure_audio_session];
			[au_holder create_audio_unit];
		}

		if ([au_holder audio_unit_state] == MSAudioUnitCreated){
			/*
			The sample rate is known only after that the AudioSession is activated.
			It may have changed compared to the original value we requested while plumbing the graph.
			*/
			handle_sample_rate_change(need_audio_session_reconfiguration);
			/* The next is done on a separate thread because it is considerably slow, so don't block the application calling thread here. */
			dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH,0), ^{
					[[AudioUnitHolder sharedInstance] configure_audio_unit];
					[[AudioUnitHolder sharedInstance] start_audio_unit:0];
			});
		}
	}else if (!activated){
		if ([au_holder audio_unit_state] == MSAudioUnitStarted) {
			[au_holder stop_audio_unit_with_param:FALSE];
		}
		if ([au_holder audio_unit_state] != MSAudioUnitNotCreated) {
			/*Mark the AudioUnit as zombified. It is unlikely to work in the future and should be recreated
			when the AudioSession will be reactivated. */
			au_holder.zombified = TRUE;
			ms_message("AudioSession is deactivated while AudioUnit was created. Mark AudioUnit as zombified.");
		}

	}
}

static void au_callkit_enabled(MSSndCard *obj, bool_t enabled) {
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	[au_holder setCallkit_enabled:enabled];
	ms_message("AudioUnit: callkit mode is %s", enabled ? "enabled" : "disabled");
	if (!enabled || TARGET_IPHONE_SIMULATOR) {
		// There is only callKit can notify audio session is activated or not.
		// So set audio session always activated when callkit is disabled.
		[au_holder setAudio_session_activated:true];
	}else{
		[au_holder setAudio_session_activated:false];
	}
}

static void au_configure(MSSndCard *obj) {
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	update_audio_unit_holder_ms_snd_card(obj);
	[au_holder configure_audio_session];
}

MSSndCardDesc au_card_desc={
.driver_type="AU",
.detect=au_detect,
.init=au_init,
.set_level=au_set_level,
.get_level=au_get_level,
.set_capture=au_set_source,
.set_control=NULL,
.get_control=NULL,
.create_reader=ms_au_read_new,
.create_writer=ms_au_write_new,
.uninit=au_uninit,
.duplicate=au_duplicate,
.usage_hint=au_usage_hint,
.audio_session_activated=au_audio_session_activated,
.callkit_enabled=au_callkit_enabled,
.audio_route_changed=au_audio_route_changed,
.configure=au_configure,
.reload_requested=au_reload_requested
};

static MSSndCard *au_card_new(const char* portName){
	MSSndCard *card = ms_snd_card_new_with_name(&au_card_desc, AUCard::portToName(portName).c_str());
	AUCard * aucard = (AUCard*)card->data;
	aucard->mPortName = portName;
	return card;
}

static MSSndCard *au_duplicate(MSSndCard *obj){
	AUCard * aucard = (AUCard*)obj->data;
	MSSndCard *card = au_card_new(AUCard::portToName(aucard->mPortName.c_str()).c_str());
	card->device_type = obj->device_type;
	return card;
}

static bool_t au_reload_requested(MSSndCardManager *m){
	return [[AudioUnitHolder sharedInstance] devices_changed_since_last_reload];
}

static void au_detect(MSSndCardManager *m){
	NSArray *inputs = [[AVAudioSession sharedInstance] availableInputs];
	int internal_id = 0;
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];

	/* Add the "default" card that follows the systems's audio route policy */
	MSSndCard *defaultCard;
	MSSndCard *card = defaultCard = au_card_new("default");
	card->device_type = MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
	card->capabilities |= MS_SND_CARD_CAP_FOLLOWS_SYSTEM_POLICY;
	ms_snd_card_set_internal_id(card, internal_id++);
	ms_snd_card_manager_add_card(m, card);

	/* Other soundcards target specific input and outputs */

	for (AVAudioSessionPortDescription *input in inputs) {
		//AvailableInputs includes devices that were disconnected within a few seconds
		if ([au_holder removedDevice].compare(std::string([input.portName UTF8String]))==0) {
			continue;
		}
		card = au_card_new([input.portName UTF8String]);
		card->device_type = deduceDeviceTypeFromAudioPortType(input.portType);
		ms_snd_card_set_internal_id(card, internal_id++);
		ms_snd_card_manager_add_card(m, card);
		ms_debug("au_detect, creating snd card %p", card);
	}

	MSSndCard *speakerCard = au_card_new(MS_SND_CARD_SPEAKER_PORT_NAME);
	speakerCard->device_type = MS_SND_CARD_DEVICE_TYPE_SPEAKER;
	ms_snd_card_set_internal_id(speakerCard, internal_id++);
	ms_snd_card_manager_add_card(m, speakerCard);
	ms_debug("au_detect -- speaker snd card %p", speakerCard);
	[au_holder setInteracted_with_bluetooth_since_last_devices_reload:FALSE];
}

static bool_t check_timestamp_discontinuity(const AudioTimeStamp *t1, const AudioTimeStamp *t2, unsigned int usualSampleCount){
	/* The *2 on usualSampleCount is to take into account small variations that arrive on the inNumberFrames values. */
	return labs((long)t1->mSampleTime - (long)t2->mSampleTime) >= (long)usualSampleCount * 2;
}



/******************** capture callback ******************/
static OSStatus au_read_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
)
{
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	[au_holder mutex_lock];
	if (![au_holder read_filter]) {
		//just return from now;
		[au_holder mutex_unlock];
		return 0;
	}
	au_filter_read_data_t *d = (au_filter_read_data_t*)[au_holder read_filter]->data;

	OSStatus err=0;
	mblk_t * rm=NULL;
	AudioBufferList readAudioBufferList;
	readAudioBufferList.mBuffers[0].mDataByteSize = inNumberFrames*[au_holder bits]/8;
	readAudioBufferList.mNumberBuffers = 1;
	readAudioBufferList.mBuffers[0].mNumberChannels = [au_holder nchannels];

	if ([au_holder read_started]){
		rm=allocb(readAudioBufferList.mBuffers[0].mDataByteSize,0);
		readAudioBufferList.mBuffers[0].mData=rm->b_wptr;
		err = AudioUnitRender([au_holder audio_unit], ioActionFlags, &d->readTimeStamp, inBusNumber,inNumberFrames, &readAudioBufferList);
		if (err == 0) {
			rm->b_wptr += readAudioBufferList.mBuffers[0].mDataByteSize;
			ms_mutex_lock(&d->mutex);
			putq(&d->rq,rm);
			d->read_samples += (readAudioBufferList.mBuffers[0].mDataByteSize / 2) / [au_holder nchannels];
			ms_mutex_unlock(&d->mutex);
			if ((readAudioBufferList.mBuffers[0].mDataByteSize / 2) / [au_holder nchannels] != inNumberFrames){
				ms_error("au_read_cb(): buffer has only %u samples !!", (unsigned) (readAudioBufferList.mBuffers[0].mDataByteSize / 2) / [au_holder nchannels]);
			}
		} else {
			check_au_unit_result(err, "AudioUnitRender");
			freeb(rm);
		}
	}else ms_warning("Read NOT started");

	if (ioData && ioData->mNumberBuffers > 1) ms_warning("ioData->mNumberBuffers=%u", (unsigned)ioData->mNumberBuffers);
	if (d->readTimeStamp.mSampleTime < 0){
		d->readTimeStamp = *inTimeStamp;
	}else{
		if (check_timestamp_discontinuity(&d->readTimeStamp, inTimeStamp, inNumberFrames)){
			ms_warning("AudioUnit capture sample time discontinuity (current=%u, previous=%u, inNumberFrames=%u) ! This usually happens during audio route changes.",
				(unsigned)inTimeStamp->mSampleTime, (unsigned)d->readTimeStamp.mSampleTime, (unsigned)inNumberFrames);
			ms_mutex_lock(&d->mutex);
			if (d->ticker_synchronizer){
				ms_ticker_synchronizer_resync(d->ticker_synchronizer);
			}
			ms_mutex_unlock(&d->mutex);
		}
		d->readTimeStamp = *inTimeStamp;
	}
	[au_holder mutex_unlock];
	return err;
}

/***************** Playback callback ***********************/
static OSStatus au_write_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
							 ) {
	ms_debug("render cb");
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];

	ioData->mBuffers[0].mDataByteSize=inNumberFrames*[au_holder bits]/8;
	ioData->mNumberBuffers=1;

	[au_holder mutex_lock];
	if(![au_holder write_filter]){
		[au_holder mutex_unlock];
		memset(ioData->mBuffers[0].mData, 0,ioData->mBuffers[0].mDataByteSize);
		return 0;
	}

	au_filter_write_data_t *d = (au_filter_write_data_t*)[au_holder write_filter]->data;

	if (d!=NULL){
		unsigned int size;

		ms_mutex_lock(&d->mutex);
		size = inNumberFrames*[au_holder bits]/8;
		//ms_message("AudioUnit playback inNumberFrames=%u timeStamp=%u", (unsigned)inNumberFrames, (unsigned)inTimeStamp->mSampleTime);
		if (d->writeTimeStamp.mSampleTime != 0 && check_timestamp_discontinuity(&d->writeTimeStamp, inTimeStamp, inNumberFrames)){
			ms_warning("AudioUnit playback discontinuity detected (current=%u, previous=%u, inNumberFrames=%u).",
				(unsigned)inTimeStamp->mSampleTime, (unsigned) d->writeTimeStamp.mSampleTime, (unsigned) inNumberFrames);
			ms_flow_controlled_bufferizer_flush(d->bufferizer);
		}
		if (ms_flow_controlled_bufferizer_get_avail(d->bufferizer) >= size) {
			ms_flow_controlled_bufferizer_read(d->bufferizer, (uint8_t *)ioData->mBuffers[0].mData, size);
		} else {
			//writing silence;
			memset(ioData->mBuffers[0].mData, 0,ioData->mBuffers[0].mDataByteSize);
			ms_debug("nothing to write, pushing silences,  framezize is %u bytes mDataByteSize %u"
					 ,inNumberFrames*[au_holder bits]/8
					 ,(unsigned int)ioData->mBuffers[0].mDataByteSize);
		}
		ms_mutex_unlock(&d->mutex);
		d->writeTimeStamp = *inTimeStamp;
	}
	[au_holder mutex_unlock];
	return 0;
}


/***********************************read function********************/

static void au_read_preprocess(MSFilter *f){
	ms_message("au_read_preprocess");
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	au_filter_read_data_t *d= (au_filter_read_data_t*)f->data;

	[au_holder check_audio_unit_is_up];
	d->ticker_synchronizer = ms_ticker_synchronizer_new();
	ms_ticker_set_synchronizer(f->ticker, d->ticker_synchronizer);
	d->read_samples_last_activity_check = (uint64_t)-1;
	[au_holder setRead_started:TRUE];
}

static void au_read_process(MSFilter *f){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	au_filter_read_data_t *d=(au_filter_read_data_t*)f->data;
	mblk_t *m;
	/*
	If audio unit is not started, it means audio session is not yet activated. Do not ms_ticker_synchronizer_update.
	*/
	if ([au_holder audio_unit_state] != MSAudioUnitStarted) return;

	ms_mutex_lock(&d->mutex);
	while((m = getq(&d->rq)) != NULL){
		ms_queue_put(f->outputs[0],m);
	}
	ms_ticker_synchronizer_update(d->ticker_synchronizer, d->read_samples, [au_holder rate]);
	ms_mutex_unlock(&d->mutex);
	if (f->ticker->time % 1000 == 0 && [au_holder mic_enabled] ){
		/* The purpose of this code is to detect when an AudioUnit becomes silent, which unfortunately happens randomly
		 * due to persistent bugs in iOS. Callkit's management of AudioSession is really unreliable, with sometimes
		 * double activations or double deactivations, deactivations shortly after beginning a second call, and sometimes
		 * AudioUnit that suddenly stops after a few seconds without any error notified to the application.
		 * That's really poor work, so mediastreamer2 has to adapt
		 */
		if (d->read_samples_last_activity_check == d->read_samples && d->read_samples_last_activity_check != (uint64_t)-1 && au_holder.stalled == FALSE){
			ms_error("Stalled AudioUnit detected, will restart it");
			au_holder.stalled = TRUE;
			dispatch_async(dispatch_get_main_queue(), ^{
					if (au_holder.stalled){
						ms_message("Stalled AudioUnit is now going to be recreated and restarted.");
						[au_holder recreate_audio_unit];
						[au_holder configure_audio_unit];
						[au_holder start_audio_unit:0];
					}
			});
		}else d->read_samples_last_activity_check = d->read_samples;
	}
}

static void au_read_postprocess(MSFilter *f){
	au_filter_read_data_t *d= (au_filter_read_data_t*)f->data;
	ms_mutex_lock(&d->mutex);
	flushq(&d->rq,0);
	ms_ticker_set_synchronizer(f->ticker, NULL);
	ms_ticker_synchronizer_destroy(d->ticker_synchronizer);
	d->ticker_synchronizer = NULL;
	[[AudioUnitHolder sharedInstance] setRead_started:FALSE];
	ms_mutex_unlock(&d->mutex);
}

/***********************************write function********************/

static void au_write_preprocess(MSFilter *f){
	ms_debug("au_write_preprocess");

	au_filter_write_data_t *d= (au_filter_write_data_t*)f->data;
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];

	[au_holder check_audio_unit_is_up];
	/*configure our flow-control buffer*/
	ms_flow_controlled_bufferizer_set_samplerate(d->bufferizer, [au_holder rate]);
	ms_flow_controlled_bufferizer_set_nchannels(d->bufferizer, [au_holder nchannels]);
	[au_holder setWrite_started:TRUE];
}

static void au_write_process(MSFilter *f){
	ms_debug("au_write_process");
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;

	if (d->base.muted || [[AudioUnitHolder sharedInstance] audio_unit_state] != MSAudioUnitStarted){
		ms_queue_flush(f->inputs[0]);
		return;
	}
	ms_mutex_lock(&d->mutex);
	ms_flow_controlled_bufferizer_put_from_queue(d->bufferizer, f->inputs[0]);
	ms_mutex_unlock(&d->mutex);
}

static void au_write_postprocess(MSFilter *f){
	ms_debug("au_write_postprocess");
	au_filter_write_data_t *d= (au_filter_write_data_t*)f->data;
	ms_mutex_lock(&d->mutex);
	ms_flow_controlled_bufferizer_flush(d->bufferizer);
	ms_mutex_unlock(&d->mutex);
	[[AudioUnitHolder sharedInstance] setWrite_started:FALSE];
}



static int read_set_rate(MSFilter *f, void *arg){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	/*The AudioSession must be configured before we decide of which sample rate we will use*/
	[au_holder configure_audio_session];
	if ((unsigned int)proposed_rate != [au_holder rate]){
		return -1;//only support 1 rate
	} else {
		return 0;
	}
}

static int write_set_rate(MSFilter *f, void *arg){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	/*The AudioSession must be configured before we decide of which sample rate we will use*/
	[au_holder configure_audio_session];
	if ((unsigned int)proposed_rate != [au_holder rate]){
		return -1;//only support 1 rate
	} else {
		return 0;
	}
}

static int get_rate(MSFilter *f, void *data){
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	[au_holder configure_audio_session];
	*(int*)data= [au_holder rate];
	return 0;
}

static int set_nchannels(MSFilter *f, void *arg){
	ms_debug("set_nchannels %d", *((int*)arg));
	[[AudioUnitHolder sharedInstance] setNchannels:*(int*)arg];
	return 0;
}

static int get_nchannels(MSFilter *f, void *data) {
	*(int *)data = [[AudioUnitHolder sharedInstance] nchannels];
	return 0;
}

static int mute_mic(MSFilter *f, void *data){
    UInt32 enableMic = *((bool *)data) ? 0 : 1;
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];

    if (enableMic == [au_holder mic_enabled])
        return 0;

	[au_holder setMic_enabled:enableMic];
    OSStatus auresult;

    if ([au_holder audio_unit_state] != MSAudioUnitStarted) {
        auresult=AudioUnitSetProperty ([au_holder audio_unit],
                                       kAudioOutputUnitProperty_EnableIO,
                                       kAudioUnitScope_Input,
                                       inputBus,
                                       &enableMic,
                                       sizeof (enableMic)
                                       );
        check_au_unit_result(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Input");
        ms_message("Enabling microphone input [%u] on non-running AudioUnit.", enableMic);
    } else {
		 ms_message("Enabling microphone input [%u] on running AudioUnit, restarted required.", enableMic);
		[au_holder stop_audio_unit_with_param:TRUE];
        [au_holder configure_audio_unit];
		[au_holder start_audio_unit:0];
    }

    return 0;
}

static int set_muted(MSFilter *f, void *data){
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	d->muted = *(int*)data;
	return 0;
}

static int audio_capture_set_internal_id(MSFilter *f, void * newSndCard)
{
	MSSndCard *oldCard = [[AudioUnitHolder sharedInstance] ms_snd_card];
	MSSndCard *newCard = (MSSndCard *)newSndCard;

	if (strcmp(newCard->name, oldCard->name)==0) {
		return 0;
	}

	if (oldCard->device_type == MS_SND_CARD_DEVICE_TYPE_SPEAKER) {
		ms_warning("audio_capture_set_internal_id(): no effect on the audio route when the current output is %s.", oldCard->name);
		return -1;
	}
	if (newCard->device_type == MS_SND_CARD_DEVICE_TYPE_SPEAKER) {
		ms_error("audio_capture_set_internal_id(): %s is not a valid input device.", newCard->name);
		return -1;
	}

	// Handle the internal linphone part with the MSSndCards
	ms_message("audio_capture_set_internal_id(): Trying to change audio input route from %s to %s", oldCard->name, newCard->name);
	update_audio_unit_holder_ms_snd_card(newCard);
	apply_sound_card_to_audio_session(newCard);
	return 0;
}

static int audio_playback_set_internal_id(MSFilter *f, void * newSndCard)
{
	MSSndCard *oldCard = [[AudioUnitHolder sharedInstance] ms_snd_card];
	MSSndCard *newCard = (MSSndCard *)newSndCard;

	// Handle the internal linphone part with the MSSndCards
	if (strcmp(newCard->name, oldCard->name)==0) {
		return 0;
	}
	ms_message("audio_playback_set_internal_id(): Trying to change audio output route from %s to %s", oldCard->name, newCard->name);
	update_audio_unit_holder_ms_snd_card(newCard);
	apply_sound_card_to_audio_session(newCard);
	return 0;
}

static MSFilterMethod au_read_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE		 , read_set_rate					},
	{	MS_FILTER_GET_SAMPLE_RATE		 , get_rate							},
	{	MS_FILTER_SET_NCHANNELS			 , set_nchannels					},
	{	MS_FILTER_GET_NCHANNELS			 , get_nchannels					},
	{	MS_AUDIO_CAPTURE_MUTE			 , mute_mic 						},
	{	MS_AUDIO_CAPTURE_SET_INTERNAL_ID , audio_capture_set_internal_id	},
	{	0				, NULL		}
};

static MSFilterMethod au_write_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE			, write_set_rate					},
	{	MS_FILTER_GET_SAMPLE_RATE			, get_rate							},
	{	MS_FILTER_SET_NCHANNELS				, set_nchannels						},
	{	MS_FILTER_GET_NCHANNELS				, get_nchannels						},
	{	MS_AUDIO_PLAYBACK_MUTE	 			, set_muted							},
	{	MS_AUDIO_PLAYBACK_SET_INTERNAL_ID 	, audio_playback_set_internal_id	},
	{	0				, NULL		}
};

static void au_read_uninit(MSFilter *f) {
	ms_message("au_read_uninit");
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	au_filter_read_data_t *d=(au_filter_read_data_t*)f->data;

	[au_holder mutex_lock];
	[au_holder setRead_filter:NULL];
	[au_holder mutex_unlock];

	destroy_audio_unit_if_not_needed_anymore();

	ms_mutex_destroy(&d->mutex);

	flushq(&d->rq,0);
	ms_free(d);
}

static void au_write_uninit(MSFilter *f) {
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];

	[au_holder mutex_lock];
	[au_holder setWrite_filter:NULL];
	[au_holder mutex_unlock];

	destroy_audio_unit_if_not_needed_anymore();

	ms_mutex_destroy(&d->mutex);
	ms_flow_controlled_bufferizer_destroy(d->bufferizer);
	ms_free(d);
}

MSFilterDesc au_read_desc={
.id=MS_IOUNIT_READ_ID,
.name="MSAURead",
.text=N_("Sound capture filter for iOS Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=0,
.noutputs=1,
.preprocess=au_read_preprocess,
.process=au_read_process,
.postprocess=au_read_postprocess,
.uninit=au_read_uninit,
.methods=au_read_methods
};


MSFilterDesc au_write_desc={
.id=MS_IOUNIT_WRITE_ID,
.name="MSAUWrite",
.text=N_("Sound playback filter for iOS Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=1,
.noutputs=0,
.preprocess=au_write_preprocess,
.process=au_write_process,
.postprocess=au_write_postprocess,
.uninit=au_write_uninit,
.methods=au_write_methods
};

// This interface gives the impression that there will be 2 different MSSndCard for the Read filter and the Write filter.
// In reality, we'll always be using a single same card for both.
static MSFilter *ms_au_read_new(MSSndCard *mscard){
	ms_message("ms_au_read_new, sound card : %p (%s)", mscard, ms_snd_card_get_name(mscard));
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	if ([au_holder read_filter] != NULL) {
		ms_fatal("Trying to create a new au_read filter when there is already one existing");
	}
	update_audio_unit_holder_ms_snd_card(mscard);
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(mscard), &au_read_desc);
	au_filter_read_data_t *d=ms_new0(au_filter_read_data_t,1);
	qinit(&d->rq);
	d->readTimeStamp.mSampleTime=-1;
	ms_mutex_init(&d->mutex,NULL);
	[au_holder setRead_filter:f];
	f->data=d;
	return f;
}

static MSFilter *ms_au_write_new(MSSndCard *mscard){
	ms_message("ms_au_write_new, sound card : %p (%s)", mscard, ms_snd_card_get_name(mscard));
	AudioUnitHolder *au_holder = [AudioUnitHolder sharedInstance];
	if ([au_holder write_filter] != NULL) {
		ms_fatal("Trying to create a new au_write filter when there is already one existing");
	}
	update_audio_unit_holder_ms_snd_card(mscard);
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(mscard), &au_write_desc);
	au_filter_write_data_t *d=ms_new0(au_filter_write_data_t,1);
	d->bufferizer= ms_flow_controlled_bufferizer_new(f, [au_holder rate], [au_holder nchannels]);
	ms_flow_controlled_bufferizer_set_max_size_ms(d->bufferizer, flowControlThreshold);
	ms_flow_controlled_bufferizer_set_flow_control_interval_ms(d->bufferizer, flowControlInterval);
	ms_mutex_init(&d->mutex,NULL);
	[au_holder setWrite_filter:f];
	f->data=d;
	return f;
}

MS_FILTER_DESC_EXPORT(au_read_desc)
MS_FILTER_DESC_EXPORT(au_write_desc)
