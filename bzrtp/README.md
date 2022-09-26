[![pipeline status](https://gitlab.linphone.org/BC/public/bzrtp/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/bzrtp/commits/master)

BZRTP
=====

What's BZRTP
------------

BZRTP is an opensource implementation of ZRTP keys exchange protocol([RFC6189](https://www.rfc-editor.org/info/rfc6189)).

The library is written in C and C++ is fully portable and can be executed  on many platforms including both ARM  processor and x86.

The library extends the ZRTP protocol version 1.1 to support Post Quantum Cryptography algorithms. See Post Quantum Crypto Engine [documentation](https://gitlab.linphone.org/BC/public/postquantumcryptoengine/-/blob/master/doc/pqcrypto_integration.pdf) for details.

License
--------

BZRTP library is dual-licensed and can be distributed either under a GNU GPLv3 license (open source, see *LICENSE.txt*) or under a proprietary
license (closed source).

Copyright © Belledonne Communications SARL

Johan Pascal is the original author of BZRTP.

Compatibility with RFC6189 - ZRTP: Media Path Key Agreement for Unicast Secure RTP
----------------------------------------------------------------------------------

### Mandatory but NOT implemented

* Sas Relay mechanism (RFC6189 [section 7.3](https://www.rfc-editor.org/rfc/rfc6189.html#section-7.3))
* Error message generation, emission or reception(which doesn't imply any security problem, they are mostly for debug purpose)


### Optional and implementd

* multistream mode
* cacheless implementation
* zrtp-hash attribute in SDP
* Go Clear/Clear ACK messages


### Optional and NOT implemented

* SAS signing


### Supported Algorithms

* Hash : SHA-256, SHA-384, SHA-512
* Cipher : AES-128, AES-256
* SAS rendering: B32, B256(PGP word list)
* Auth Tag : HS32, HS80
* Key Agreement : DH-2048, DH-3072, X25519, X448

*Notes*: 
* X25519 and X448 Key agreements(RFC7748) are not part of RFC6189 and supported only when *bctoolbox[1]* is linking *libdecaf[2]*
* SHA-512 hash is not part of RFC6189

### Extension
In order to support Post Quantum Key Encapsulation Mechanisms, the original ZRTP protocol was extended to include a KEM mode key agreement.

When Post Quantum Cryptography is enabled, the library also supports the following hybrids:
* X255/Kyber512, X255/HQC128, X255/Kyber512/HQC128
* X448/Kyber1024, X448/HQC256, X448/Kyber1024/HQC256


Dependencies
------------

- *bctoolbox[1]*: portability layer and crypto function abstraction
- *libdecaf[2]*: X25519 and X448 implementation. *bzrtp* does not link directly with libdecaf but uses it through *bctoolbox*
- *PostQuantumCryptoEngine[3]*: KEM hybrid scheme and ECDH-based KEM, PQC crypto abstraction layer
- *sqlite3[4]*: requested to support key continuity


Build BZRTP
-----------
```
cmake . -DCMAKE_INSTALL_PREFIX=<prefix> -DCMAKE_PREFIX_PATH=<search_paths>
	
make
make install
```

Build options
-------------

* `CMAKE_INSTALL_PREFIX=<string>` : install prefix
* `CMAKE_PREFIX_PATH=<string>`    : column-separated list of prefixes where to search for dependencies
* `ENABLE_SHARED=NO`              : do not build the shared library
* `ENABLE_STATIC=NO`              : do not build the static library
* `ENABLE_STRICT=NO`              : build without the strict compilation flags
* `ENABLE_TESTS=YES`              : build non-regression tests
* `ENABLE_DOC=NO`                 : generates API documentation
* `ENABLE_PACKAGE_SOURCE=NO`      : create package source target for source archive making

* `ENABLE_ZIDCACHE=YES`           : support cache mechanism, enable key continuity. Requires *sqlite3*
* `ENABLE_GOCLEAR=YES`            : support GoClear packets (see RFC6189 [section 4.7.2](https://www.rfc-editor.org/rfc/rfc6189.html#section-4.7.2))
* `ENABLE_PQCRYPTO=NO`            : support KEM mode extension and Post Quantum Crypto algorithms


Notes for packagers
-------------------

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.

### Rpm packaging

bzrtp package can be generated with cmake using the following command:
```
mkdir WORK
cd WORK
cmake ../
make package_source
rpmbuild -ta --clean --rmsource --rmspec bzrtp-<version>-<release>.tar.gz
```



----------------------------------


* [1] <https://gitlab.linphone.org/BC/public/bctoolbox>
* [2] <https://sourceforge.net/projects/ed448goldilocks/>
* [3] <https://gitlab.linphone.org/BC/public/postquantumcryptoengine>
* [4] <https://gitlab.linphone.org/BC/public/external/sqlite3>
