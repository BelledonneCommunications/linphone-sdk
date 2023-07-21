# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- Dynamic protection of video streams against packet loss with flexible forward error correction (FEC), depending on the bandwidth and the loss rate.

## [5.3.0] - 2023-12-18

### Added
- AV1 codec added
- SRTP GCM mode
- Add MS_FILTER_IS_HW_ACCELERATED MSFilter flag to advertise hw-accelerated codecs
- Dummy codec, to avoid fatal errors when a encoder/decoder is not available
- Double SRTP encryption (experimental)

### Changed
- Removed legacy platform abstraction code, now provided by bctoolbox.
- Various optimisations for video processing on Android platform

### Fixed
- Fix buffer overflow in GenericPLC filter.
- Issue with H264 decoding of malformed RTP streams

## [5.2.0] - 2022-11-14

### Added
- Video conferencing features
  * RFC6464 client-to-mixer
  * RFC6465 mixer-to-client
- Video conference: added CSRC of contributor to active speaker stream.
- Add support for post-quantum encryption algorithms within ZRTP protocol.

### Changed
- Licence becomes AGPL-3.
- Improvements to Android bluetooth audio support.
- Performance improvements to Android Texture video renderer.
- Optimizations to DTLS handshake to be more robust to packet losses.
- libyuv used for rescaling and pixel conversion (instead of libswscale).

### Fixed
- Crash with buffer overflow in GenericPLC filter.
- Random crashes with video capture on Windows.
- ARM64/linux compilation.
- MSVideoRouter algorithm improved.
- Non working audio playback on iOS 16.
- Blurry video rendering on iOS.


## [5.1.0] - 2022-02-14

### Added
- Experimental Forward Error Correction feature for video streams.

### Changed
- Mkv and Wav file player and recorders file I/O is tunneled through bctbx VFS API.

### Fixed
- See commits for details.


## [5.0.0] - 2021-07-08

### Added
- API to change media encryption in the fly

### Changed
- refactoring and simplifications for the MSOgl filter (video OpenGL rendering)

### Fixed
- video corruption when Android MediaCodec runs out of buffers.
- absence of dummy stun packets when DTLS is used with "encryption_mandatory" preventing 
  DTLS handshare to take place



## [4.5.0] - 2021-03-29

### Added
- Audio route API to choose which device to use for capture & playback on Android & iOS.
- New camera capture backend based on Windows MediaFoundation 
- MSMediaPlayer enhancements.
- VideoConference API and engine prototype (active speaker switching only)


### Changed
- audio flow control algorithm improved, silent portions are dropped first.
- MKVPlayer supports seek in files without cue table.
- 'packetlosspercentage' is now configurable in opus encoder.

### Fixed
- misfunction in DTLS handshake over TURN
- fix arythmetic issue in clock skew computation, causing bad audio when the sound device outputs 
  audio fragments not multiple of 10ms.
- iOS AudioUnit configured with bad sample rate issue.
- wrong selection of ICE TURN candidates when the TURN server is behind a firewall
- unsent TURN SenderIndication packets on iOS
- fix video freeze in VP8, due to lack of retransmission of SLIs.
All these fix were backported to 4.4 branch.

## [4.4.0] - 2020-06-16

### Added
- TURN over TCP or TLS (previously was only UDP)
- Capture/playback gain control for Mac OS (AudioUnit)

### Changed
- Optimize mirroring for OpenGL based display filters. It was previously done in software.
- Make V4L2 capture filter work with read-only devices (such as with v4lloopback driver)
- iOS AudioUnit filter simplifications

### Fixed
- ICE: set source IP addresses of outgoing packets consistent with the local candidate it is supposed to test or use.
  This fixes various ICE failures occuring when the host has multiple IP addresses (which is now common with IPv6)
- New implementation of MSEventQueue, to fix unreproductible crashes with the previous implementation.
- Crashes around mblk_t management of Video4Linux2 catpure filter.
- Random crash in VideoToolbox decoding filter.
- VP8 decoding errors due to an invalid aggregation of packets in rare circumstances
- Crash while reading mkv file with checksums.


## [4.3.0] -  2019-10-14


### Added
- H265 codec based on MediaCodec API (Android only)
- H265 codec based on VideoToolbox API (iOS only)
- Adaptation of video resolution according to network capabilities.

### Changed
- License is now GNU GPLv3.
- For simplicity reasons, version number is aligned between all components of linphone-sdk.


## [2.16.1] - 2017-07-21

### Fixed
- build on windows



## [2.16.0] - 2017-07-20

### Changed
- Ticker is driven by soundcards.
- Soundcard flow control improvements.



## [2.15.1] - 2017-03-02

## Fixed
 - issue in CMake scripts.


 
## [2.15.0] - 2017-02-23

### Added
- support for TURN (RFC5766).
- IPv6 support for ICE

### Removed
- Deprecation of QTKit for video capture management on Apple platforms.

### Fixed
- Multiple issues around H264 hardware encoder/decoder on iOS/MacOSX/Android.


## [2.13.0] - 2016-06-02

### Added
- H264 hardware codec on Apple's platforms
- BroadVoice16 audio codec support.

### Changed
- MSFactory usage : using one MSfactory per LinphoneCore instance, deprecate all "static" methods, now grouped around the MSFactory object.

### Fixed
- Fix option to really disable libv4l2 when asked.



## [2.12.0] - 2015-11-02

### Added
- Basic AVPF handling for H.264 codec.
- Support of video presets (include a high-fps preset).
- Support of RTP session as input/output of a audio/video stream (instead of a sound card or camera/display).
- Video presets (include a high-fps preset)
- Ability to use an RTP session as input/output of a audio/video stream (instead of a sound card or camera/display)
- Handling of jpeg image on Windows 10
- Video capture and display filter for BlackBerry 10
- Add text stream for RTT (Real-Time Text)
- VP8 recording/playing in mkv files

### Changed
- Allow video stream to keep its source (camera).


## [2.11.0] - 2015-03-11

### Added
- AVPF with VP8 codec only.
- Matroska file format (needs libmatroska2).
- Audio/video stream recorder. Only H264 is supported for video stream.
- New API methods to send audio/video streams read from a file (only WAV and MKV file formats are supported).
- New API methods to play multimedia files and display to a specified drawing surface.
- Support of multicast IP addresses.
- Support of SBR for AAC codec (iOS only).


## [2.10.0] - 2014-02-20

### Added
- HD video support.
- new OpenSLES android sound module.
- Opus codec.

### Changed
- update android AEC settings table.


## [2.9.0] - 2013-05-27

### Added
- ICE support (RFC5245).
- Accessors to set DSCP parameters for media streams.
- AudioStream recording feature.
- OpenGL video output for Linux.
- Stereo support for L16 codec.
- AAC-ELD codec integration for iOS
- integration with acoustic echo canceller from WebRTC
- pre-calibrated device latency table to configure echo canceller

### Changed
- Split the libmediastreamer library in two libraries: libmediastreamer_base and
  libmediastreamer_voip. For VoIP support, both libraries must be linked to the executable.
- API change to the audio_stream_new, video_stream_new, audio_stream_start_full,
		video_stream_start functions to use different addresses for RTP and RTCP.
	
- Adaptive bitrate control improvements
- Faster call quality indicator feedback
 



## [2.8.0] - 2011-12-22

### Added
- Audio conferencing.
- Mac OS X video support.
- New adaptive audio & video bitrate control api.
- New call quality indicator api.


## [2.7.0] - 2011-02-07

### Added
- Android video capture filter.
- Android video display filters: one for 2.1 and one for 2.2+.
- Scaler/colorspace conversion abstraction, with native ARM-optimized implementation.
- X11+XvXshm display filter, deprecating SDL display filter.
- Custom tone generation (in MSDtmfGen filter).
- Custom tone detection in new filter MSToneDetector

### Changed
- Enhance build for visual studio.
- Rework the echo canceller to work with much less latency.
- adapt the OSS filter to OSS4 standard.


## [2.6.0] - 2010-07-01

### Added
- Optional "threaded" v4l2 capture.

### Changed
- Android sound capture optimisations.
- Move H264 decoder from msx264 to mediastreamer2.
- Echo canceller reworked: use soundcard stream to synchronise far-end stream.
- H263 RFC2190 support improvements.
- MSVolume improvements and cleanup, with native AGC support.

### Fixed
- Crash when video window is closed on windows.
- Segfault in ALSA support when capturing a stereo stream.

## [2.5.0] - 2010-06-03

### Added
- Event queue for notifications of MSFilters.
- Stereo support to resampler.
- New MSFilter to convert from mono to stereo and vice versa.
- Inter Ticker Communication filter (ITC) so that graphs running on different MSTicker can exchange data.
- Audio mixer filter to mix down audio streams. This is not suitable for conferencing, use MSConf instead.

### Changed
- Uses less memory for speex decoding.

### Fixed
- Regression with speex decoder at 16 and 32khz.


## [2.4.0] - 2010-05-19

### Added
- jpeg over RTP support.
- PulseAudio support.
- New MSDrawDibDisplay video output filter with new layout features.

### Changed
- Use libv4l2 when possible to benefit from hardware pixel conversion.
- Enhance performance of SDL video output
- Improve MacOS sound support


## [2.3.0] - 2009-09-17

### Added
- Parametric equalizer filter (to modify gains per frequency bands), working with natural curves.
- Noise-gate feature added to MSVolume.
- Builds on windows with mingw/msys using ./configure && make (see linphone's README.mingw).

### Changed
- Integrate directshow capture filter for mingw (was a plugin before).
- List of soundcard dynamically updates on windows upon device plugs/unplugs.
- MSVolume echo limiter feature improved.


## [2.2.3] - 2009-01-21

### Added
- New MSWebcam object to provide Webcam management and MSFilter instantiation.

### Changed
- Rfc3984 support improved .
- Webcam support on windows largely improved (vfw mode).
- Support for configuring video size up to svga.
- Video output can automatically resize to the size of received video stream.

### Fixed
- Fix crash when resizing video window.
- Alsa issues.


## [2.2.2] - 2008-10-06

### Added
- winsnd3.c file for support of soundcard under windows (seems to work a bit better).


## [2.2.1] - 2008-01-25

## Added
- Snow codec.
- Enable setting of max rtp payload size for all encoders.
- Video output resizing.
- 4CIF and VGA support.


## [2.2.0] - 2007-11-19

### Added
- "No webcam" screen.
- REQ_VFU command to request a video encoder to send an I-frame (implemented for ffmpeg based encoders).
- Contributed macosx sound support.
- New MSVolume filter to make sound power measurements.
- rate control of ffmpeg video codecs.

### Changed
- Bandwidth settings improvements.


## [2.1.0] - 2007-01-23

### Added
- Support for Video4Linux V2 cameras.
- Support for mpjeg cameras.
- Webcam support on windows operational.
- Video window display ok on windows.

### Changed
- Bandwidth setting improvements.

### Fixed
- Fix bug with quickcam driver on linux.

