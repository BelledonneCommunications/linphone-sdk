Lime
=======

Lime is a C++ library implementing Open Whisper System Signal protocol :
Sesame, double ratchet and X3DH.
It is designed to work jointly with *Linphone*[3]

Dependencies
------------
- *bctoolbox[1]* : portability layer, built with Elliptic Curve Cryptography
- *bellesip[2]* : for the https stack
- *soci-sqlite3* : Db access


Build instrucitons
------------------

 cmake . -DCMAKE_INSTALL_PREFIX=<install_prefix> -DCMAKE_PREFIX_PATH=<search_prefix>

 make
 make install

Testing
-------
 To test on local machine, you must run a local X3DH server.
 A nodejs version of UNSECURE X3DH server is provided in tester/server
 See README from this directory for instructions.

 A test instance of X3DH server shall be running on sip5.linphone.org.


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
------------------

- [1] bctoolbox: git://git.linphone.org/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox>
- [2] belle-sip: git://git.linphone.org/belle-sip.git or <https://www.linphone.org/releases/sources/belle-sip>
- [3] linphone-desktop: git://git.linphone.org/linphone-desktop.git
