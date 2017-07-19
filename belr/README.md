What's Belr
===========

Belr is Belledonne Communications' language recognition library.
It aims at parsing any input formatted according to a language defined by an ABNF grammar,
such as the protocols standardized at IETF.

It is based on finite state machine theory and heavily relies on recursivity from an implementation standpoint.


Dependencies
============

- *bctoolbox[1]*: our portability layer


Build Belr
==========

		cmake . -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_PREFIX_PATH=<search_prefixes>
		
		make
		make install


Build options
=============

* `CMAKE_INSTALL_PREFIX=<string>`: install prefix
* `CMAKE_PREFIX_PATH=<string>`: column-separated list of prefixes where to search for dependencies
* `ENABLE_SHARED=NO`: do not build the shared library
* `ENABLE_STATIC=NO`: do not build the static library
* `ENABLE_STRICT=NO`: build without strict compilation flags (-Wall -Werror)
* `ENABLE_TOOLS=NO`: do not build tools (belr-demo, belr-parse)


Note for packagers
==================

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.

Rpm packaging
belr can be generated with cmake3 using the following command:
mkdir WORK
cd WORK
cmake3 ../
make package_source
rpmbuild -ta --clean --rmsource --rmspec belr-<version>-<release>.tar.gz



-----------------------

* [1] git://git.linphon.org/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox>
