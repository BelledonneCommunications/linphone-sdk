Mediastreamer
=============

* Project    : mediastreamer2 - a modular sound and video processing and streaming
* Email      : <simon.morlat@linphone.org>
* License    : GPLv2(or later) or Commercial licensing
* Home Page  : <http://www.mediastreamer2.com>

Commercial support and licensing is provided by Belledonne Communications
<http://www.belledonne-communications.com>

Mediastreamer2 is a library to make audio and
video real-time streaming and processing. Written in pure C,
it is based upon the ortp library.

Design
------

Using mediastreamer2 will allow you to chain filters in a graph. Each
filter will be responsible for doing some kind of processing and will
deliver data to the next filter. As an example, you could get some
data from network and unpack it in an RTP filter. This RTP filter will
deliver the data to a decoder (speex, G711...) which will deliver it
to a filter that is able to play the PCM data or record it into a .wav
file.

There is a doxygen documentation for more information.

Compilation and installation
----------------------------

### Required dependencies:

- **bctoolbox[1]**: portability layer
- **oRTP[2]**: RTP stack
- **libspeexdsp**: echo cancelation feature (disablable)
- **ffmpeg** or libav: H263 codec, MPEG4 decodec and RAW picture rescaling (disablable)

### Optional dependencies

- **libsrtp** for SRTP encryption
- **bzrtp[3]** for ZRTP encryption
- **libgsm**: GSM codecs support
- **libbv16**: BV16 codec support
- **libopus** for Opus encoding and decoding
- **libspeex**: SPEEX codec support
- **libalsa**: ALSA support (GNU/Linux only)
- **libpulse**: PulseAudio support (GNU/Linux only)
- **libv4l2**: video capture (GNU/Linux only;disablable)
- **libx11** and libxv: video display with X11 (GNU/Linux only)
- **libglx**: video dispaly with GLX (GNU/Linux only)
- **libvpx** for VP8 encoding and decoding
- **libmastroska-c** for recording/playing of audio/video streams
- **libturbojpeg**: video screenshot feature

### Build instructions:

The Autotools way is deprecated. Use [CMake](https://cmake.org) to configure the source code.

	cmake . -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_PREFIX_PATH=<search_path>
	
	make
	make install

#### Supported opitions:

- `CMAKE_INSTALL_PREFIX=<string>` : install prefix
- `CMAKE_PREFIX_PATH=<string>`    : column-separated list of prefixes where to search for dependencies
- `ENABLE_SHARED=NO`              : do not build the shared library
- `ENABLE_STATIC=NO`              : do not build the static library
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


- [1] git://git.linphone.org/bctoolbox.git *or* <http://www.linphone.org/releases/sources/bctoolbox>
- [2] git://git.linphone.org/ortp.git *or* <http://www.linphone.org/releases/sources/ortp>
- [3] git://git.linphone.org/bzrtp.git *or* <http://www.linphone.org/releases/sources/bzrtp>
