Project    : mediastreamer2 - a modular sound and video processing and streaming
Email      : simon.morlat_at_linphone.org
License    : GPLv2(or later) or Commercial licensing
Home Page  : http://www.mediastreamer2.com

Commercial support and licensing is provided by Belledonne Communications
http://www.belledonne-communications.com

Mediastreamer2 is a library to make audio and
video real-time streaming and processing. Written in pure C,
it is based upon the ortp library.

Design:
-------------------------------------

Using mediastreamer2 will allow you to chain filters in a graph. Each
filter will be responsible for doing some kind of processing and will
deliver data to the next filter. As an example, you could get some
data from network and unpack it in an RTP filter. This RTP filter will
deliver the data to a decoder (speex, G711...) which will deliver it
to a filter that is able to play the PCM data or record it into a .wav
file.

There is a doxygen documentation for more information.

Compilation and installation
-------------------------------------

* Required dependencies:
	- oRTP
	- libspeex and libspeexdsp
	- bctoolbox
* Optional dependencies (for video to be enabled, see --enable-video):
	- libavcodec
	- libswscale
	- libvpx
	- libopus
	- x11 with libxv-dev on linux
	- libmastroska2 from https://github.com/Matroska-Org/foundation-source
	- theora
	- libsrtp

  * For Linux, MacOS or mingw
   $> ./autogen.sh
   $> ./configure
   $> make
   $> su -c 'make install'

   More instructions and advices can be found for the mingw compilation procedure in Linphone's README.mingw.

  * For Windows with Visual Studio

   It is recommended to use the linphone-desktop project (git://git.linphone.org/linphone-desktop.git) and follow the README file found there. If you really want to
   build mediastreamer2 only you can pass the -DLINPHONE_BUILDER_TARGET=ms2 option to the prepare.py script.

Environment variables used by mediastreamer2
---------------------------------------------

MS2_RTP_FIXED_DELAY : default value is 0. When set to 1, RTP packets belonging from one tick execution are actually sent at the beginning of the next tick.
This allows a zero jitter in the RTP timing at sender side. This is to be used for measurements, this mode has no interest for doing a real conversation and does not improve 
quality.

MS_AUDIO_PRIO, MS_VIDEO_PRIO : define the scheduling policy of the audio and video threads (MSTicker objects). Possible values are 'NORMAL', 'HIGH', 'REALTIME'.
The corresponding behavior is as follows:
	Priority type     |    Linux              |       MacOS           |         Windows 
	NORMAL            |SCHED_OTHER, def. prio |SCHED_OTHER, def. prio | Default priority.
	HIGH              |SCHED_RR, max prio     |SCHED_RR, max prio     | THREAD_PRIORITY_HIGHEST
	REALTIME          |SCHED_FIFO, max prio   |SCHED_FIFO, max prio   | THREAD_PRIORITY_HIGHEST
	Note that SCHED_FIFO leaves entire control of the cpu to the mediastreamer2 thread. In case of CPU overload due to heavy encoder processing for example,
	a mono-core machine will stop responding.

MS_TICKER_SCHEDPRIO : UNIX only. It is an integer defining the thread priority to be used by MSTicker. Values are OS specific and depend on the scheduling policy
SCHED_FIFO, SCHED_RR or SCHED_OTHER.

MS2_OPUS_COMPLEXITY : opus codec complexity parameter. A value of -1 stands for mediastreamer2's own default value. Otherwise it must be between 0 and 10.

MEDIASTREAMER_DEBUG : when set to 1, verbose logging is activated by default.

DISPLAY : used by video display filters relying on X11 (linux only).


Notes about third parties compilation
-------------------------------------

* libmatroska2:
 - get the source with
	$ git clone git://git.linphone.org/libmatroska-c.git
 - compilation:
	$ make -f Makefile
	$ make -f Makefile install


Contact information:
-------------------------------------

Use the *linphone* mailing list for question about mediastreamer2.
  <linphone-developers@nongnu.org>.
Subscribe here:
  https://savannah.nongnu.org/mail/?group=linphone


