[![pipeline status](https://gitlab.linphone.org/BC/public/mediastreamer2/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/mediastreamer2/commits/master)

Mediastreamer2
==============

Mediastreamer2 is a powerful and lightweight streaming engine for voice/video telephony applications.
This media processing and streaming toolkit is responsible for receiving and sending all multimedia streams in Linphone, including voice/video capture, encoding and decoding, and rendering.

For additional information, please [visit mediastreamer2's homepage on **linphone.org**](https://www.linphone.org/en/voip-media-engine/)

License
-------

Copyright © Belledonne Communications

Mediastreamer2 is dual licensed, and is available either :

 - under a [GNU/AGPLv3 license](https://www.gnu.org/licenses/agpl-3.0.html), for free (open source). Please make sure that you understand and agree with the terms of this license before using it (see LICENSE.txt file for details). AGPLv3 is choosen over GPLv3 because mediastreamer2 can be used to create server-side applications, not just client-side ones. Any product incorporating mediastreamer2 to provide a remote service then has to be licensed under AGPLv3.
 For a client-side product, the "remote interaction" clause of AGPLv3 being irrelevant, the usual terms GPLv3 terms do apply (the two licences differ by this only clause).

 - under a proprietary license, for a fee, to be used in closed source applications. Contact [Belledonne Communications](https://www.linphone.org/contact) for any question about costs and services.

Documentation
-------------

Mediastreamer2 has a concept of filters, that can be connected to form a graph. Each
filter is responsible for doing some kind of processing and 
delivers data to the next filter. As an example, you could get some
data from network and unpack it in an RTP filter. This RTP filter will
deliver the data to a decoder (speex, G711...) which will deliver it
to a filter that is able to play the PCM data or another to record it into a .wav
file.
A more high level API is available in mediastreamer2/mediastream.h header file, exposing
primitives to create audio and video streams suitable for a VoIP application.



Compilation and installation
----------------------------

### Required dependencies:

- **bctoolbox[1]**: portability layer
- **oRTP[2]**: RTP stack

### Optional dependencies

- **libsrtp** for SRTP encryption
- **bzrtp[3]** for ZRTP encryption
- **libvpx** for VP8 encoding and decoding
- **aom** for AV1 encoding
- **dav1d** for AV1 decoding
- **libmastroska-c** for recording/playing of audio/video streams
- **libopus** for Opus encoding and decoding
- **libspeex**: SPEEX codec support
- **libspeex-dsp: resampler and AEC
- **libturbojpeg**: video screenshot feature
- **libalsa**: ALSA support (GNU/Linux only)
- **libpulse**: PulseAudio support (GNU/Linux only)
- **libgsm**: GSM codecs support
- **libbv16**: BV16 codec support
- **libv4l2**: video capture (GNU/Linux only;disablable)
- **libx11** and libxv: video display with X11 (GNU/Linux only)
- **libglx**: video dispaly with GLX (GNU/Linux only)
- **ffmpeg**, h264 decoder, mpeg4 and mjpeg encoder/decoders, rescaling and pixel conversion (unmaintained)

### Build instructions:

The Autotools way is deprecated. Use [CMake](https://cmake.org) to configure the source code.

	cmake . -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_PREFIX_PATH=<search_path>
	
	make
	make install
	
Alternatively, mediastreamer2 library is integrated in *linphone-sdk[4]* meta project, which provides a convenient way
to build it for various targets.

#### Supported options:

- `CMAKE_INSTALL_PREFIX=<string>` : install prefix
- `CMAKE_PREFIX_PATH=<string>`    : column-separated list of prefixes where to search for dependencies
- `ENABLE_STRICT=NO`              : build without strict compilation flags (-Wall -Werror)
- `ENABLE_UNIT_TESTS=YES`         : build tester binaries
- `ENABLE_DOC=NO`                 : do not generate the documentation
- `ENABLE_DEBUG_LOGS=YES`         : turn on debug-level logs


#### Note for packagers:

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.

Rpm packaging
mediastremer2 rpm can be generated with cmake3 using the following command:
mkdir WORK
cd WORK
cmake3 ../
make package_source
rpmbuild -ta --clean --rmsource --rmspec mediastreamer-<version>-<release>.tar.gz



Environment variables used by mediastreamer2
--------------------------------------------

`MS2_RTP_FIXED_DELAY` : default value is 0. When set to 1, RTP packets belonging from one tick execution are actually sent at the beginning of the next tick.
This allows a zero jitter in the RTP timing at sender side. This is to be used for measurements, this mode has no interest for doing a real conversation and does not improve 
quality.

`MS_AUDIO_PRIO`, `MS_VIDEO_PRIO` : define the scheduling policy of the audio and video threads (MSTicker objects). Possible values are 'NORMAL', 'HIGH', 'REALTIME'.
The corresponding behavior is as follows:

	+-------------------+------------------------+------------------------+-------------------------+
	| Priority type     |     GNU/Linux          |        MacOS           |         Windows         |
	+-------------------+------------------------+------------------------+-------------------------+
	| NORMAL            | SCHED_OTHER, def. prio | SCHED_OTHER, def. prio | Default priority.       |
	| HIGH              | SCHED_RR, max prio     | SCHED_RR, max prio     | THREAD_PRIORITY_HIGHEST |
	| REALTIME          | SCHED_FIFO, max prio   | SCHED_FIFO, max prio   | THREAD_PRIORITY_HIGHEST |
	+-------------------+------------------------+------------------------+-------------------------+

Note that `SCHED_FIFO` leaves entire control of the cpu to the mediastreamer2 thread. In case of CPU overload
due to heavy encoder processing for example, a mono-core machine will stop responding.

- `MS_TICKER_SCHEDPRIO` : UNIX only. It is an integer defining the thread priority to be used by MSTicker. Values
                          are OS specific and depend on the scheduling policy `SCHED_FIFO`, `SCHED_RR` or `SCHED_OTHER`.
- `MS2_OPUS_COMPLEXITY` : opus codec complexity parameter. A value of -1 stands for mediastreamer2's own default value.
                          Otherwise it must be between 0 and 10.
- `MEDIASTREAMER_DEBUG` : when set to 1, verbose logging is activated by default.
- `DISPLAY`             : used by video display filters relying on X11 (GNU/Linux only).



Contact information
-------------------

Use the *linphone* mailing list for question about mediastreamer2.
  <linphone-developers@nongnu.org>.
Subscribe here:
  <https://savannah.nongnu.org/mail/?group=linphone>


--------------------------------------


- [1] bctoolbox: https://gitlab.linphone.org/BC/public/bctoolbox
- [2] ortp: https://gitlab.linphone.org/BC/public/ortp
- [3] bzrtp: https://gitlab.linphone.org/BC/public/bzrtp
- [4] linphone-sdk: https://gitlab.linphone.org/BC/public/linphone-sdk.git
