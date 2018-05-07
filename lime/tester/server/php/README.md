README for the X3DH test server running on nginx/php/mysql docker images
========================================================================

Disclaimer
-----------
THIS SERVER IS NOT INTENDED FOR REAL USE BUT FOR TEST PURPOSE ONLY
DO NOT USE IN REAL LIFE IT IS NOT SECURE.

Mostly, it is missing clients authentication. Providing a client
authentication layer before calling the x3dh_process_request function
SHOULD be enough to use it in a real situation.

As usual, this code is distributed in the hope it will be usefull but
without any warranty or implied warranty.


Requirements
------------
- docker-compose

Install
-------
**Permissions:**
Check that the ./source/var/log directory is writable by anyone

**Certificates:**
The X3DH servers use ./source/cn/x3dh-cert.pem and source/cn/x3dh-key.pem
By default it is the certificate available to test client in tester/data

**Database:**
The database is accessible through phpmyadmin on localhost:8080
User     : root
Password : root

To reset it, just delete the content of ./db/mysql directory.

**Images version:**
All images points to version more or less matching the latest
at the time this is being written.


Content and Settings
--------------------

./.env : docker-compose environment variables (https://docs.docker.com/compose/environment-variables/#the-env-file)
    - images version
    - mysql root password and host name
    - nginx host name

./customphp.ini : the php.ini configuration file
    - make sure the error display is turned off
    - set the path to error logs: /source/var/log/php-error.log

./site.conf: the nginx sites configuration file spawning:
    - http://localhost:8080 the phpmyadmin access
    - https://localhost:25519 and https://localhost:25520 the X3DH test servers

./docker-compose.yml : set images for
    - nginx
    - php
    - phpmyadmin (used to help debugging if needed)
    - mysql : acces on port 3306, user: root, password: root

./db/mysql/ : directory used by mysql container to store all its data.
    - When container is down, erase the content of this to force a DB reset at next up

./db/initdb.d/ : contains SQL scripts executed at mysql container setup (first intanciation of the mysql container)
    - x3dh25519.sql : set up the database for the x3dh-25519.php server
    - x3dh448.sql : set up the database for the x3dh-448.php server

./source/cn/: self signed certificate used by https server

./source/x3dh/ : php sources of X3DH server.
    - x3dh-25519.php : the settings file for the x3dh server running on curve 25519
    - x3dh-448.php : the settings file for the x3dh server running on curve 448
    - x3dh.php : the actual server source
    - x3dh-createBase.php : a script to create the X3DH server database as needed.
            It is not active by default and not used, just provided as example.

./source/var/log : directory were the x3dh server and php logs are written, make sur it is writable
    - php-error.log : the php logs
    - X3DH25519.log : logs from X3DH 25519 server
    - X3DH448.log : logs from X3DH 448 server


Usage
-----
start:
docker-compose up -d
(run without -d options to see full logs in the console, can be wise for the first runs)

This command run 2 X3DH servers accessible at:
- https://localhost:25519 using curve25519
- https://localhost:25520 using curve448

stop:
docker-compose down --volumes
