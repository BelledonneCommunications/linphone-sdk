# LINPHONE CMAKE BUILDER #

Linphone CMake builder is a structure based on CMake to build linphone and
flexisip on various platforms.

## BUILD PREREQUISITES

The common prerequisites to use linphone-cmake-builder are:
- to intall CMake >= 3.0 (available in the package systems of Linux 
distributions, in MacPorts for Mac OS X (http://www.macports.org/) and/or from 
http://cmake.org/
- to install git (available in the package systems of Linux distributions, in 
MacPorts for Mac OS X (http://www.macports.org/) and/or from http://cmake.org/
- on Windows, you also need 7-zip to generate SDK packages
(http://www.7-zip.org/)

## AVAILABLE BUILD TARGETS

The following targets are supported:
- Linphone for desktop platforms - see README.desktop.md
- Liblinphone Python extension module - see README.python.md
- Liblinphone Python extension module for Raspberry PI
- Flexisip and dependencies - see README.flexisip.md
- Linphone for iOS - see README.ios.md

## GETTING HELP ON BUILD OPTIONS WHATEVER THE BUILD TARGET IS

	$ ./prepare.py --help

## OpenH264 support
OpenH264 support is enabled in all generated installers and SDK archive.
However, the OpenH264 runtime library must be downloaded directly from
Cisco's website to be complient with H264 patents. In the case of linphone
for desktop, it is automatically done by the Windows and MacOS X installers.
In the case of SDKs of liblinphone and mediastreamer, you must manualy
download the OpenH264 archive and extract it in the "bin" directory of the SDK
distribution and rename the DLL file into "openh264.dll".

The patent-free OpenH264 runtime library archive is available at
http://ciscobinary.openh264.org/openh264-1.4.0-win32msvc.dll.bz2
