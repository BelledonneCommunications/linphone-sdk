MsOpenH264
==========

Overview
--------

MsOpenH264 is an  H.264 encoder/decoder plugin for mediastreamer2 based on the openh264 library.


Compilation guide
-----------------

### Dependencies

- *Mediastreamer[1]:* needed for its API
- [OpenH264][openh264-website]: H264 encoder and decoder


### Build procedure

The Autotools way is deprecated. Use [CMake][cmake-website] to configure the source code.

	cmake . -DCMAKE_INSTALL_PRFIX=<prefix> -DCMAKE_PREFIX_PATH=<search_prefixes>
	
	make
	make install


### Build options

- `CMAKE_INSTALL_PREFIX=<string>` : installation prefix
- `CMAKE_PREFIX_PATH=<string>`    : column-separated list of prefixes where to look for dependencies
- `ENABLE_DECODER=NO`             : disable H264 decoding feature


### Note for packagers

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.


-----------------------------------

[1] Mediastreamer: git://git.linphone.org/mediastreamer2.git *or* <http://www.linphone.org/releases/sources/mediastreamer>


[openh264-website]: http://www.openh264.org/
[cmake-website]: https://cmake.org
