# LINPHONE CMAKE BUILDER #

Linphone CMake builder is a structure based on CMake to build linphone on 
various platforms.

## BUILD PREREQUISITES

The common prerequisites to use linphone-cmake-builder are:
- to intall CMake >= 2.8.12 (available in the package systems of Linux 
distributions, in MacPorts for Mac OS X (http://www.macports.org/) and/or from 
http://cmake.org/
- to install git (available in the package systems of Linux distributions, in 
MacPorts for Mac OS X (http://www.macports.org/) and/or from http://cmake.org/
- you also need 7-zip to generate SDK packages (http://www.7-zip.org/)

## AVAILABLE BUILD TARGETS

The following targets are supported:
- Linphone for desktop platforms
- Linphone for BlackBerry10 - see README.bb10
- Liblinphone Python extension module - see README.python
- Flexisip and dependencies - see README.flexisip.md

## OpenH264 support
OpenH264 support is enabled in all generated installers and SDK archive.
However, the OpenH264 runtime library must be downloaded directly from
Cisco's websit to be complient with H264 patents. In the case of linphone GTK+,
it is automatically done by the Windows and MacOS installers. In the case of
SDKs of linphone and mediastreamer, you must manualy download the OpenH264 archive
and extract it in the "bin" directory of the SDK distribution and rename the DLL
file into "openh264.dll".

The pattent free OpenH264 runtime library archive is available here:
http://ciscobinary.openh264.org/openh264-1.4.0-win32msvc.dll.bz2