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
  --setting, -s        path to configuration file
                                              [string] [default:multiserver.cfg]
  --port, -p           port to listen                  [number] [default: 25519]
  --certificate, -c    path to server certificate
                                             [string] [default: "x3dh-cert.pem"]
  --key, -k            path to server private key
                                              [string] [default: "x3dh-key.pem"]
  --resource_dir, -r   set directory path to used as base to find database and
                       key files                        [string] [default: "./"]
  --lifetime, -l       lifetime of a user in seconds, 0 is forever.
                                                         [number] [default: 300]

Setting
-------
Configuration file must export an associative array
    - CurveId => Db Path
For all the curve Id enabled on the server
Default is to enable all the available ones:
    - c25519
    - c448
    - c25519/kyber512
    - c25519/mlkem512
    - c488/mlkem1024

Script
------
To run automated tests if you build liblime with all algorithms enabled you must
support all the available base algorithm.
Note: multidomains and lime server test suites are not supported by the default configuration

A convenient launching script is provided : localServerStart.sh
It does:
- killall nodejs instance running(beware if you use it no shared server)
- wipe out c25519.sqlite3, c448.sqlite3, c25519k512.sqlite3, c25519mlk512.sqlite and c448mlk1024.sqlite
- start a server supporting all the available base algorithms using the previous files as DB
