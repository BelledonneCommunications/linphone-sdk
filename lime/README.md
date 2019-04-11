Lime
=======

Lime is a thread safe C++ library implementing Open Whisper System Signal protocol :
Sesame, double ratchet and X3DH. https://signal.org/docs/

Lime can run the Signal Protocol using elliptic curve 25519 or curve 448-goldilocks.

It is designed to work jointly with *Linphone*[1] in a multiple devices per user and multiple users per device environment.


License
--------
Copyright (c) 2019 Belledonne Communications SARL under GNU GPLv3 (see *LICENSE.txt*)

The above license excludes the content of *src/jni* directory Copyright (c) 2016 Mapbox covered by its respective license,
see *src/jni/LICENSE.txt*

Dependencies
------------
- *bctoolbox[2]* : portability layer, built with Elliptic Curve Cryptography
- *soci-sqlite3* : Db access


Build instrucitons
------------------
```
 cmake -DCMAKE_INSTALL_PREFIX=<install_prefix> -DCMAKE_PREFIX_PATH=<search_prefix> <path_to_source>

 make
 make install
```

Documentation
-------------

To generate the Doxygen documentation files(having ran the cmake command):

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
- `ENABLE_SHARED`                 : build the shared library (default YES)
- `ENABLE_STATIC`                 : build the static library (default YES)
- `ENABLE_STRICT`                 : build with strict complier flags e.g. `-Wall -Werror` (default YES)
- `ENABLE_CURVE25519`             : Enable support of Curve 25519 (default YES)
- `ENABLE_CURVE448`               : Enable support of Curve 448 (default YES)
- `ENABLE_PROFILING`              : Enable code profiling for GCC (default NO)
- `ENABLE_C_INTERFACE`            : Enable support of C89 foreign function interface (default NO)
- `ENABLE_JNI`                    : Enable support of Java foreign function interface (default NO)

------------------

- [1] linphone-desktop: https://gitlab.linphone.org/BC/public/linphone-desktop.git
- [2] bctoolbox: https://gitlab.linphone.org/BC/public/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox>
