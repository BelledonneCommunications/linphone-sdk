# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Audio route API to choose which device to use for capture & playback on Android & iOS.

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

