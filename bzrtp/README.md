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
* key agreement DH2048


### Optional and NOT implemented

* zrtp-hash attribute in SDP
* Go Clear/Clear ACK messages
* SAS signing


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


----------------------------------


* [1] git://git.linphone.org/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox>
