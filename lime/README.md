Lime
=======

LIME is an end-to-end encryption library for one-to-one and group instant messaging, allowing users to exchange messages privately and asynchronously. It uses modern ciphering curve X448 and double ratchet algorithm for perfect forward secrecy.

LIME is composed of a portable client library coupled with a public key server developed by Belledonne Communications to allow end-to-end encryption for messaging, without having to exchange cryptographic keys simultaneously.

The library exposes a C, C++, Python and Java API for easy integration in mobile and desktop environments.

LIME supports multiple devices per user and multiple users per device.

For additional information, please [visit LIME's page on **linphone.org**](http://www.linphone.org/technical-corner/lime)

License
--------

LIME library is dual-licensed and can be distributed either under a GNU GPLv3 license (open source, see *LICENSE.txt*) or under a proprietary
license (closed source).

Copyright © Belledonne Communications SARL

The content of *src/jni* directory Copyright (c) 2016 Mapbox are covered by its respective license,
see *src/jni/LICENSE.txt*

The public key server (LIME server) is not part of this software package, and is only available under a proprietary license.

Dependencies
------------
- *bctoolbox[2]* : portability layer, built with Elliptic Curve Cryptography
- *soci-sqlite3* : Db access


Build instructions
------------------
```
 cmake -DCMAKE_INSTALL_PREFIX=<install_prefix> -DCMAKE_PREFIX_PATH=<search_prefix> <path_to_source>

 make
 make install
```

Alternatively, LIME library is integrated in *linphone-sdk* meta project, which provides a convenient way
to build it for various targets.

Documentation
-------------

To generate the Doxygen documentation files (having ran the cmake command with ENABLE_DOC On):

```
 make doc
```

Comprehensive documentation on implementation choices and built-in protocols in *lime.pdf*


Testing
-------
To test on local machine, you must run a local X3DH server.
 - A nodejs/sqlite version of UNSECURE X3DH server is provided in *tester/server/nodejs*
 See README from this directory for instructions.

If a local X3DH server is running with default parameter just run
```
 make test
```

To launch a particular test suite, use ctest, running:
```
 ctest -R <Test suite name> [--verbose]
```

Test suite names are
- *crypto*: Test the crypto primitives
- *double_ratchet*: Specific to the Double Ratchet protocol
- *hello_world*: Basic Lime test, mostly a demo code. Requires a live X3DH server on localhost
- *lime*: Complete testing of all features provided by the library. Requires a live X3DH server on localhost
- *multidomains*: Tests specific to the server multidomain management. Requires a live X3DH server on localhost, does not work with the nodejs server provided with the library.
- *server*: Tests some server configuration(resource usage limitation). Requires a live X3DH server on localhost, does not work with the nodejs server provided with the library.
- *C_ffi*: Available only if C interface is enabled, test the C89 foreign function interface
- *JNI*: Available only if JNI is enabled, test the Java foreign function interface


Library settings
----------------
Some mostly harmless settings are available in *src/lime_settings.hpp*


Library APIs
-----------
The C++11 API is available in *include/lime/lime.hpp*

if enabled (see Options), a C89 FFI is provided by *include/lime/lime_ffi.h*

if enabled (see Options), a java FFI is provided by *Lime.jar* located in the build directory in *src/java* subdirectory


Options
-------

- `CMAKE_INSTALL_PREFIX=<string>` : installation prefix
- `CMAKE_PREFIX_PATH=<string>`    : prefix where depedencies are installed
- `ENABLE_UNIT_TESTS`             : compile non-regression tests (default YES)
- `ENABLE_STRICT`                 : build with strict complier flags e.g. `-Wall -Werror` (default YES)
- `ENABLE_CURVE25519`             : Enable support of Curve 25519 (default YES)
- `ENABLE_CURVE448`               : Enable support of Curve 448 (default YES)
- `ENABLE_PROFILING`              : Enable code profiling for GCC (default NO)
- `ENABLE_C_INTERFACE`            : Enable support of C89 foreign function interface (default NO)
- `ENABLE_JNI`                    : Enable support of Java foreign function interface (default NO)
- `ENABLE_DOC`                    : Enable documenation generation, requires Doxygen (default NO)

------------------

- [1] linphone-desktop: https://gitlab.linphone.org/BC/public/linphone-desktop.git
- [2] bctoolbox: https://gitlab.linphone.org/BC/public/bctoolbox.git
- [3] linphone-sdk: https://gitlab.linphone.org/BC/public/linphone-sdk.git
