[![pipeline status](https://gitlab.linphone.org/BC/public/belle-sip/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/belle-sip/commits/master)

Belle-sip
=========

Overview
--------

Belle-sip is a modern library implementing SIP (RFC3261) transport, transaction and dialog layers.
It is written in C, with an object oriented API.
It also comprises a simple HTTP/HTTPS client implementation.

License
-------

<Copyright 2012 © Belledonne Communications SARL>, all rights reserved.

Belle-sip is dual licensed:
- under a GNU GPLv3 license for free (see LICENSE.md file for details)
- under a proprietary license, for closed source projects. Contact <sales@belledonne-communications.com> for costs and other service information.


Features
--------

- RFC3261 compliant implementation of SIP parser, writer, transactions and dialog layers.
- SIP transaction state machines with lastest updates (RFC6026).
- fully asynchronous transport layer (UDP, TCP, TLS), comprising DNS resolver (SRV, A, AAAA).
- full dual-stack IPv6 support.
- automatic management of request refreshes with network disconnection resiliency thanks to the "refresher" object.
- supported platforms: Linux, Mac OSX, windows XP+, iOS, Android, Blackberry 10.
- HTTP/HTTPS client implementation.

Dependencies
------------

### Build-time dependencies

These are required to generate a C sourcefile from SIP grammar using [antlr3](http://www.antlr3.org/) generator:

* [Java SE](http://www.oracle.com/technetwork/java/javase/downloads/index.html) or OpenJDK
* [antlr 3.4](https://github.com/antlr/website-antlr3/blob/gh-pages/download/antlr-3.4-complete.jar)


### Runtime dependencies

- *libantlr3c* version 3.2 or 3.4 - but its source is included in belle-sip, no need to install it.
- *bctoolbox* (git://git.linphone.org/bctoolbox.git or <https://gitlab.linphone.org/BC/public/bctoolbox>)



Building belle-sip with CMake
-----------------------------

	cmake .
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

