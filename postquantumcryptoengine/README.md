[![pipeline status](https://gitlab.linphone.org/BC/private/postquantumcryptoengine/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/private/postquantumcryptoengine/commits/master)

PostQuantumCryptoEngine
=======================

Extension to the bctoolbox lib providing Post Quantum Cryptography.
Provides:
- Kyber 512, 768 and 1024
- HQC 128, 192 and 256 (NIST round 3 version)
- X25519 and X448 in KEM version
and a way to combine two or more of theses.

Licensing
---------

Copyright © Belledonne Communications

PostQuantumCryptoEngine is dual licensed, and is available either :

 - under a [GNU/GPLv3 license](https://www.gnu.org/licenses/gpl-3.0.en.html), for free (open source). Please make sure that you understand and agree with the terms of this license before using it (see LICENSE.txt file for details).

 - under a proprietary license, for a fee, to be used in closed source applications. Contact [Belledonne Communications](https://www.linphone.org/contact) for any question about costs and services.


Depends
-------

- **liboqs[1]**: implementation of a collection of Post Quantum algorithms
- **bctoolbox[2]**


To compile
----------

	cmake . -DCMAKE_INSTALL_PREFIX=<install prefix> -DCMAKE_PREFIX_PATH=<search prefix>

	make
	make install


To make an rpm package
----------------------

	cmake . -DCMAKE_INSTALL_PREFIX=/usr -DCPACK_GENERATOR="RPM"

	make package


Options
-------

- `CMAKE_INSTALL_PREFIX=<string>`: install prefix.
- `CMAKE_PREFIX_PATH=<string>`: search path prefix for dependencies e.g. mbedtls.
- `ENABLE_SHARED=NO`: do not build the shared libraries.
- `ENABLE_STATIC=NO`: do not build the static libraries.
- `ENABLE_STRICT=NO`: do not build with strict compilator flags e.g. `-Wall -Werror`.
- `ENABLE_TESTS=NO`: do not build testing binaries.



Note for packagers
------------------

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.

--------------------

- [1] <https://gitlab.linphone.org/BC/public/external/liboqs>
- [2] <https://gitlab.linphone.org/BC/public/bctoolbox>

