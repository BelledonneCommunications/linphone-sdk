Lime
=======

Lime is a C++ library implementing Open Whisper System Signal protocol :
Sesame, double ratchet and X3DH. https://signal.org/docs/

Lime can run the Signal Protocol using elliptic curve 25519 or curve 448-goldilocks.

It is designed to work jointly with *Linphone*[1] in a multiple devices per user and multiple users per device environment.

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

 - A php/mysql (on docker) version of UNSECURE X3DH server is provided in *tester/server/php*
 See README from this directory for instructions.

Library settings
----------------
Some mostly harmless settings are available in *src/lime_settings.hpp*

Library API
-----------
The C++11 API is available in *include/lime/lime.hpp*

if enabled (see Options), a C89 FFI is provided by *include/lime/lime_ffi.h*


Options
-------

- `CMAKE_INSTALL_PREFIX=<string>` : installation prefix
- `CMAKE_PREFIX_PATH=<string>`    : prefix where depedencies are installed
- `ENABLE_UNIT_TESTS=NO`          : do not compile non-regression tests
- `ENABLE_SHARED=NO`              : do not build the shared library.
- `ENABLE_STATIC=NO`              : do not build the static library.
- `ENABLE_STRICT=NO`              : do not build with strict complier flags e.g. `-Wall -Werror`
- `ENABLE_Curve25519`             : Enable support of Curve 25519.
- `ENABLE_Curve448`               : Enable support of Curve 448.
- `ENABLE_PROFILING`              : Enable code profiling for GCC
- `ENABLE_C_INTERFACE`            : Enable support of C89 foreign function interface

------------------

- [1] linphone-desktop: git://git.linphone.org/linphone-desktop.git
- [2] bctoolbox: git://git.linphone.org/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox>
