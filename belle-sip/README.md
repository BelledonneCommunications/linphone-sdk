# Belle-sip

## Overview

Belle-sip is a SIP (RFC3261) implementation written in C, with an object oriented API.
Please check "NEWS" file for an overview of current features.
Copyright 2012-2014, Belledonne Communications SARL <contact@belledonne-communications.com>, all rights reserved.

Belle-sip is distributed to everyone under the GNU GPLv2 (see COPYING file for details).
Incorporating belle-sip within a closed source project is not possible under the GPL.
Commercial licensing can be purchased for that purpose from [Belledonne Communications](http://www.belledonne-communications.com).

## Build prerequisite

* [Java SE](http://www.oracle.com/technetwork/java/javase/downloads/index.html) on openJDK
 This is required to generate a C sourcefile from SIP grammar using [antlr3](http://www.antlr3.org/) generator.

### Dependencies

* libtool
* intltool
* pkg-config
* libantlr3c-3.2 or 3.4
* antlr3-3.4
* bctoolbox (see below)
* C++ compiler (for instance g++ or clang++)
* (optional) BCUnit

#### Under Debian/Ubuntu

        apt-get install libtool intltool pkg-config libantlr3c-dev antlr3 g++ make
        #and for optional dependencies
        apt-get install libbcunit1-dev libpolarssl-dev

#### Under MacOS X using HomeBrew

        brew install intltool libtool pkg-config automake libantlr3.4c homebrew/versions/antlr3
        ln -s /usr/local/bin/glibtoolize /usr/local/bin/libtoolize

#### Under Windows using mingw and Visual Studio

 The procedure is tested for Visual Studio Express 2012.

 * Compile and install libantlr3c, BCUnit with ./configure && make && make install
 * get antlr3 from linphone's git server (see above). This version contains up to date visual studio project and solution files.
 * get BCUnit from linphone's git server (see above). This version contains up to date visual studio project and solution files.
 * put belle-sip next to antlr3 and to bcunit (in the same directory).
 * open belle-sip/build/windows/belle-sip-tester/belle-sip-tester.sln or belle-sip/build/windows/belle-sip/belle-sip.sln
 * Build the solution (antlr3 and bcunit are built automatically)

#### Building bctoolbox

bctoolbox build system is Cmake only, but provides pkg-config file for integration with autotools.
git://git.linphone.org/bctoolbox.git 


#### Known issues

 1. Antlr3 on windows
  On windows you have to edit /usr/local/include/antl3defs.h replace:

         #include <winsock.h>
  with:

        #include <winsock2.h>
  Or get the source code from linphone's git (linphone branch):

        git clone -b linphone git://git.linphone.org/antlr3.git
        git clone -b linphone git://git.linphone.org/bcunit.git








