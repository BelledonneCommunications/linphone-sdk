[![pipeline status](https://gitlab.linphone.org/BC/public/bzrtp/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/bzrtp/commits/master)

BZRTP
=====

What's BZRTP
------------

BZRTP is an opensource implementation of ZRTP keys exchange protocol. 
The library written in C 89 is fully portable and can be executed  on many platforms including both ARM  processor and x86. 

Licensing: The source code is licensed under GPLv2.


Compatibility with RFC6189 - ZRTP: Media Path Key Agreement for Unicast Secure RTP
----------------------------------------------------------------------------------

### Mandatory but NOT implemented

* Sas Relay mechanism (section 7.3)
* Error message generation, emission or reception(which doesn't imply any security problem, they are mostly for debug purpose)


### Optional and implementd

* multistream mode
* cacheless implementation
* zrtp-hash attribute in SDP


### Optional and NOT implemented

* Go Clear/Clear ACK messages
* SAS signing


### Supported Algorithms

* Hash : SHA-256, SHA-384
* Cipher : AES-128, AES-256
* SAS rendering: B32, B256(PGP word list)
* Auth Tag : HS32, HS80
* Key Agreement : DH-2048, DH-3072, X25519, X448

*Note*: X25519 and X448 Key agreements(RFC7748) are not part of RFC6189 and supported only when *bctoolbox[1]* is linking *libdecaf[2]*

Dependencies
------------

- *bctoolbox[1]*: portability layer and crypto function abstraction


Build BZRTP
-----------

	cmake . -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_PREFIX_PATH=<search_paths>
	
	make
	make install


Build options
-------------

* `CMAKE_INSTALL_PREFIX=<string>` : install prefix
* `CMAKE_PREFIX_PATH=<string>`    : column-separated list of prefixes where to search for dependencies
* `ENABLE_SHARED=NO`              : do not build the shared library
* `ENABLE_STATIC=NO`              : do not build the static library
* `ENABLE_STRICT=NO`              : build without the strict compilation flags
* `ENABLE_TESTS=YES`              : build non-regression tests


Notes for packagers
-------------------

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.

Rpm packaging
bzrtp can be generated with cmake3 using the following command:
mkdir WORK
cd WORK
cmake3 ../
make package_source
rpmbuild -ta --clean --rmsource --rmspec bzrtp-<version>-<release>.tar.gz



----------------------------------


* [1] git://git.linphone.org/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox>
* [2] <https://sourceforge.net/projects/ed448goldilocks/>
