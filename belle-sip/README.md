Belle-sip
=========

Overview
--------

Belle-sip is a SIP (RFC3261) implementation written in C, with an object oriented API.
Please check "NEWS" file for an overview of current features.
Copyright 2012-2014, Belledonne Communications SARL <contact@belledonne-communications.com>, all rights reserved.

Belle-sip is distributed to everyone under the GNU GPLv2 (see COPYING file for details).
Incorporating belle-sip within a closed source project is not possible under the GPL.
Commercial licensing can be purchased for that purpose from [Belledonne Communications](http://www.belledonne-communications.com).

Dependencies
------------

### Build-time dependencies

These are required to generate a C sourcefile from SIP grammar using [antlr3](http://www.antlr3.org/) generator:

* [Java SE](http://www.oracle.com/technetwork/java/javase/downloads/index.html) or OpenJDK
* [antlr 3.4](https://github.com/antlr/website-antlr3/blob/gh-pages/download/antlr-3.4-complete.jar)


### Runtime dependencies

- *libantlr3c* version 3.2 or 3.4
- *bctoolbox* (git://git.linphone.org/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox/>)


### Under Debian/Ubuntu

		apt-get install libantlr3c-dev antlr3


### Under MacOS X using HomeBrew

		brew install libantlr3.4c homebrew/versions/antlr3


Building bctoolbox with CMake
-----------------------------

		cmake . -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_PREFIX_PATH=<search_prefix>
	
		make
		make install


Build options
-------------

* `CMAKE_INSTALL_PREFIX=<string>` : install prefix.
* `CMAKE_PREFIX_PATH=<string>`    : column-separated list of prefixes where to find dependencies.
* `ENABLE_TESTS=NO`               : disable non-regression tests.
* `ENABLE_STRICT=NO`              : build without strict build options like `-Wall -Werror`
* `ENABLE_SHARED=NO`              : do not build the shared library
* `ENABLE_STATIC=NO`              : do not build the static library


Note for packagers
------------------

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.

Rpm packaging
belle-sip can be generated with cmake3 using the following command:
mkdir WORK
cd WORK
cmake3 ../
make package_source
rpmbuild -ta --clean --rmsource --rmspec belle-sip-<version>-<release>.tar.gz

