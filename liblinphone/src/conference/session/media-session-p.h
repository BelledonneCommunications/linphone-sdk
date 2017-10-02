/*
 * media-session-p.h
 * Copyright (C) 2017  Belledonne Communications SARL
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MEDIA_SESSION_P_H_
#define _MEDIA_SESSION_P_H_

#include <utility>

#include "call-session-p.h"

#include "media-session.h"
#include "port-config.h"
#include "nat/ice-agent.h"
#include "nat/stun-client.h"

#include "linphone/call_stats.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class MediaSessionPrivate : public CallSessionPrivate {
public:
	MediaSessionPrivate (const Conference &conference, const CallSessionParams *params, CallSessionListener *listener);
	virtual ~MediaSessionPrivate ();

public:
	static void stunAuthRequestedCb (void *userData, const char *realm, const char *nonce, const char **username, const char **password, const char **ha1);

	void accepted () override;
	void ackReceived (LinphoneHeaders *headers) override;
	bool failure () override;
	void pausedByRemote ();
	void remoteRinging () override;
	void resumed ();
	void terminated () override;
	void updated (bool isUpdate);
	void updating (bool isUpdate) override;

	void enableSymmetricRtp (bool value);
	void sendVfu ();

	void clearIceCheckList (IceCheckList *cl);
	void deactivateIce ();
	void prepareStreamsForIceGathering (bool hasVideo);
	void stopStreamsForIceGathering ();

	int getAf () const {
		return af;
	}

	bool getAudioMuted () const {
		return audioMuted;
	}

	LinphoneCore *getCore () const {
		return core;
	}

	IceSession *getIceSession () const {
		return iceAgent->getIceSession();
	}

	SalMediaDescription *getLocalDesc () const {
		return localDesc;
	}

	MediaStream *getMediaStream (LinphoneStreamType type) const;
	LinphoneNatPolicy *getNatPolicy () const {
		return natPolicy;
	}

	int getRtcpPort (LinphoneStreamType type) const;
	int getRtpPort (LinphoneStreamType type) const;
	LinphoneCallStats *getStats (LinphoneStreamType type) const;
	int getStreamIndex (LinphoneStreamType type) const;
	int getStreamIndex (MediaStream *ms) const;
	SalCallOp * getOp () const { return op; }
	void setAudioMuted (bool value) { audioMuted = value; }

private:
	static OrtpJitterBufferAlgorithm jitterBufferNameToAlgo (const std::string &name);

	#ifdef VIDEO_ENABLED
		static void videoStreamEventCb (void *userData, const MSFilter *f, const unsigned int eventId, const void *args);
	#endif // ifdef VIDEO_ENABLED
	#ifdef TEST_EXT_RENDERER
		static void extRendererCb (void *userData, const MSPicture *local, const MSPicture *remote);
	#endif // ifdef TEST_EXT_RENDERER
	static void realTimeTextCharacterReceived (void *userData, MSFilter *f, unsigned int id, void *arg);

	static float aggregateQualityRatings (float audioRating, float videoRating);

	void setState (LinphoneCallState newState, const std::string &message) override;

	void computeStreamsIndexes (const SalMediaDescription *md);
	void fixCallParams (SalMediaDescription *rmd);
	void initializeParamsAccordingToIncomingCallParams () override;
	void setCompatibleIncomingCallParams (SalMediaDescription *md);
	void updateBiggestDesc (SalMediaDescription *md);
	void updateRemoteSessionIdAndVer ();

	void initStats (LinphoneCallStats *stats, LinphoneStreamType type);
	void notifyStatsUpdated (int streamIndex) const;

	OrtpEvQueue *getEventQueue (int streamIndex) const;
	MediaStream *getMediaStream (int streamIndex) const;
	MSWebCam *getVideoDevice () const;

	void fillMulticastMediaAddresses ();
	int selectFixedPort (int streamIndex, std::pair<int, int> portRange);
	int selectRandomPort (int streamIndex, std::pair<int, int> portRange);
	void setPortConfig (int streamIndex, std::pair<int, int> portRange);
	void setPortConfigFromRtpSession (int streamIndex, RtpSession *session);
	void setRandomPortConfig (int streamIndex);

	void discoverMtu (const Address &remoteAddr);
	std::string getBindIpForStream (int streamIndex);
	void getLocalIp (const Address &remoteAddr);
	std::string getPublicIpForStream (int streamIndex);
	void runStunTestsIfNeeded ();
	void selectIncomingIpVersion ();
	void selectOutgoingIpVersion ();

	void forceStreamsDirAccordingToState (SalMediaDescription *md);
	bool generateB64CryptoKey (size_t keyLength, char *keyOut, size_t keyOutSize);
	void makeLocalMediaDescription ();
	int setupEncryptionKey (SalSrtpCryptoAlgo *crypto, MSCryptoSuite suite, unsigned int tag);
	void setupDtlsKeys (SalMediaDescription *md);
	void setupEncryptionKeys (SalMediaDescription *md);
	void setupRtcpFb (SalMediaDescription *md);
	void setupRtcpXr (SalMediaDescription *md);
	void setupZrtpHash (SalMediaDescription *md);
	void transferAlreadyAssignedPayloadTypes (SalMediaDescription *oldMd, SalMediaDescription *md);
	void updateLocalMediaDescriptionFromIce ();

	SalMulticastRole getMulticastRole (SalStreamType type);
	void joinMulticastGroup (int streamIndex, MediaStream *ms);

	int findCryptoIndexFromTag (const SalSrtpCryptoAlgo crypto[], unsigned char tag);
	void setDtlsFingerprint (MSMediaStreamSessions *sessions, const SalStreamDescription *sd, const SalStreamDescription *remote);
	void setDtlsFingerprintOnAllStreams ();
	void setupDtlsParams (MediaStream *ms);
	void setZrtpCryptoTypesParameters (MSZrtpParams *params);
	void startDtls (MSMediaStreamSessions *sessions, const SalStreamDescription *sd, const SalStreamDescription *remote);
	void startDtlsOnAllStreams ();
	void updateCryptoParameters (SalMediaDescription *oldMd, SalMediaDescription *newMd);
	bool updateStreamCryptoParameters (const SalStreamDescription *localStreamDesc, SalStreamDescription *oldStream, SalStreamDescription *newStream, MediaStream *ms);

	int getIdealAudioBandwidth (const SalMediaDescription *md, const SalStreamDescription *desc);
	int getVideoBandwidth (const SalMediaDescription *md, const SalStreamDescription *desc);
	RtpProfile *makeProfile (const SalMediaDescription *md, const SalStreamDescription *desc, int *usedPt);
	void unsetRtpProfile (int streamIndex);
	void updateAllocatedAudioBandwidth (const PayloadType *pt, int maxbw);

	void applyJitterBufferParams (RtpSession *session, LinphoneStreamType type);
	void clearEarlyMediaDestination (MediaStream *ms);
	void clearEarlyMediaDestinations ();
	void configureAdaptiveRateControl (MediaStream *ms, const OrtpPayloadType *pt, bool videoWillBeUsed);
	void configureRtpSessionForRtcpFb (const SalStreamDescription *stream);
	void configureRtpSessionForRtcpXr (SalStreamType type);
	RtpSession *createAudioRtpIoSession ();
	RtpSession *createVideoRtpIoSession ();
	void freeResources ();
	void handleIceEvents (OrtpEvent *ev);
	void handleStreamEvents (int streamIndex);
	void initializeAudioStream ();
	void initializeStreams ();
	void initializeTextStream ();
	void initializeVideoStream ();
	void parameterizeEqualizer (AudioStream *stream);
	void prepareEarlyMediaForking ();
	void postConfigureAudioStream (AudioStream *stream, bool muted);
	void postConfigureAudioStreams (bool muted);
	void setPlaybackGainDb (AudioStream *stream, float gain);
	void setSymmetricRtp (bool value);
	void startAudioStream (LinphoneCallState targetState, bool videoWillBeUsed);
	void startStreams (LinphoneCallState targetState);
	void startTextStream ();
	void startVideoStream (LinphoneCallState targetState);
	void stopAudioStream ();
	void stopStreams ();
	void stopTextStream ();
	void stopVideoStream ();
	void tryEarlyMediaForking (SalMediaDescription *md);
	void updateFrozenPayloads (SalMediaDescription *result);
	void updateStreams (SalMediaDescription *newMd, LinphoneCallState targetState);
	void updateStreamsDestinations (SalMediaDescription *oldMd, SalMediaDescription *newMd);

	bool allStreamsAvpfEnabled () const;
	bool allStreamsEncrypted () const;
	bool atLeastOneStreamStarted () const;
	void audioStreamAuthTokenReady (const std::string &authToken, bool verified);
	void audioStreamEncryptionChanged (bool encrypted);
	uint16_t getAvpfRrInterval () const;
	unsigned int getNbActiveStreams () const;
	bool isEncryptionMandatory () const;
	int mediaParametersChanged (SalMediaDescription *oldMd, SalMediaDescription *newMd);
	void propagateEncryptionChanged ();

	void fillLogStats (MediaStream *st);
	void updateLocalStats (LinphoneCallStats *stats, MediaStream *stream) const;
	void updateRtpStats (LinphoneCallStats *stats, int streamIndex);

	bool mediaReportEnabled (int statsType);
	bool qualityReportingEnabled () const;
	void updateReportingCallState ();
	void updateReportingMediaInfo (int statsType);

	void executeBackgroundTasks (bool oneSecondElapsed);
	void reportBandwidth ();
	void reportBandwidthForStream (MediaStream *ms, LinphoneStreamType type);

	void abort (const std::string &errorMsg) override;
	void handleIncomingReceivedStateInIncomingNotification () override;
	bool isReadyForInvite () const override;
	LinphoneStatus pause ();
	void setTerminated () override;
	LinphoneStatus startAcceptUpdate (LinphoneCallState nextState, const std::string &stateInfo) override;
	LinphoneStatus startUpdate (const std::string &subject = "") override;
	void terminate () override;
	void updateCurrentParams () const override;

	void accept (const MediaSessionParams *params);
	LinphoneStatus acceptUpdate (const CallSessionParams *csp, LinphoneCallState nextState, const std::string &stateInfo) override;

	#ifdef VIDEO_ENABLED
		void videoStreamEventCb (const MSFilter *f, const unsigned int eventId, const void *args);
	#endif // ifdef VIDEO_ENABLED
	void realTimeTextCharacterReceived (MSFilter *f, unsigned int id, void *arg);

	void stunAuthRequestedCb (const char *realm, const char *nonce, const char **username, const char **password, const char **ha1);

private:
	static const std::string ecStateStore;
	static const int ecStateMaxLen;

	MediaSessionParams *params = nullptr;
	mutable MediaSessionParams *currentParams = nullptr;
	MediaSessionParams *remoteParams = nullptr;

	AudioStream *audioStream = nullptr;
	OrtpEvQueue *audioStreamEvQueue = nullptr;
	LinphoneCallStats *audioStats = nullptr;
	RtpProfile *audioProfile = nullptr;
	RtpProfile *rtpIoAudioProfile = nullptr;
	int mainAudioStreamIndex = LINPHONE_CALL_STATS_AUDIO;

	VideoStream *videoStream = nullptr;
	OrtpEvQueue *videoStreamEvQueue = nullptr;
	LinphoneCallStats *videoStats = nullptr;
	RtpProfile *rtpIoVideoProfile = nullptr;
	RtpProfile *videoProfile = nullptr;
	int mainVideoStreamIndex = LINPHONE_CALL_STATS_VIDEO;
	void *videoWindowId = nullptr;
	bool cameraEnabled = true;

	TextStream *textStream = nullptr;
	OrtpEvQueue *textStreamEvQueue = nullptr;
	LinphoneCallStats *textStats = nullptr;
	RtpProfile *textProfile = nullptr;
	int mainTextStreamIndex = LINPHONE_CALL_STATS_TEXT;

	LinphoneNatPolicy *natPolicy = nullptr;
	StunClient *stunClient = nullptr;
	IceAgent *iceAgent = nullptr;

	// The address family to prefer for RTP path, guessed from signaling path.
	int af;

	std::string mediaLocalIp;
	PortConfig mediaPorts[SAL_MEDIA_DESCRIPTION_MAX_STREAMS];
	bool needMediaLocalIpRefresh = false;

	// The rtp, srtp, zrtp contexts for each stream.
	MSMediaStreamSessions sessions[SAL_MEDIA_DESCRIPTION_MAX_STREAMS];

	SalMediaDescription *localDesc = nullptr;
	int localDescChanged = 0;
	SalMediaDescription *biggestDesc = nullptr;
	SalMediaDescription *resultDesc = nullptr;
	bool expectMediaInAck = false;
	unsigned int remoteSessionId = 0;
	unsigned int remoteSessionVer = 0;

	std::string authToken;
	bool authTokenVerified = false;
	std::string dtlsCertificateFingerprint;

	unsigned int nbMediaStarts = 0;

	// Upload bandwidth setting at the time the call is started. Used to detect if it changes during a call.
	int upBandwidth = 0;

	// Upload bandwidth used by audio.
	int audioBandwidth = 0;

	bool allMuted = false;
	bool audioMuted = false;
	bool automaticallyPaused = false;
	bool pausedByApp = false;
	bool playingRingbackTone = false;
	bool recordActive = false;

	std::string onHoldFile;

	L_DECLARE_PUBLIC(MediaSession);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _MEDIA_SESSION_P_H_
