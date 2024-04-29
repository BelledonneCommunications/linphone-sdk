README for the X3DH test server running on nodejs
=================================================

Disclaimer
-----------
THIS SERVER IS NOT INTENDED FOR REAL USE BUT FOR TEST PURPOSE ONLY
DO NOT USE IN REAL LIFE IT IS NOT SECURE


Requirements
------------
- nodejs
- sqlite3
- certificate(default x3dh-cert.pem)
- associated private key(default x3dh-key.pem)

The certificate must also be accessible to tester client.

Install
-------
run :
npm install

in the current directory


Usage
-----
node x3dh.js -d <path> [options]
Options:
  --help               Show help                                       [boolean]
  --version            Show version number                             [boolean]
  --database, -d       path to database file                 [string] [required]
  --port, -p           port to listen                  [number] [default: 25519]
  --certificate, -c    path to server certificate
                                             [string] [default: "x3dh-cert.pem"]
  --key, -k            path to server private key
                                              [string] [default: "x3dh-key.pem"]
  --ellipticCurve, -e  set which Elliptic Curve users of this server must use,
                       option used once at database creation, it is then ignored
                        [string] [choices: "c25519", "c448"] [default: "c25519"]
  --resource_dir, -r   set directory path to used as base to find database and
                       key files                        [string] [default: "./"]
  --lifetime, -l       lifetime of a user in seconds, 0 is forever.
                                                         [number] [default: 300]

Script
------
Server is managing one type of keys(Curve 25519 or Curve 448 based).
To run automated tests if you build liblime with all algorithms enabled you must
run three servers, on ports 25519, 25520 and 25526

A convenient launching script is provided : localServerStart.sh
It does:
- killall nodejs instance running(beware if you use it no shared server)
- wipe out c25519.sqlite3, c448.sqlite3 and c25519k512.sqlite3
- start server using c22519.sqlite3 db listening on port 25519 using curve25519
- start server using c448.sqlite3 db listening on port 25520 using curve448
- start server using c25519k512.sqlite3 db listening on port 25526 using curve25519k512
