# LIBLINPHONE SDK FOR BLACKBERRY 10 USING LINPHONE CMAKE BUILDER #

## BUILD PREREQUISITES

The common prerequisites listed in the README file are requested to build the 
liblinphone SDK for BlackBerry 10.

You will also need to install the BlackBerry 10 native SDK (Momentics IDE). You 
can get it from http://developer.blackberry.com/native/downloads/
/!\ The Blackberry SDK MUST BE at the root of the user home directory and named bbndk /!\

Note that building the liblinphone SDK for BlackBerry 10 requires to be 
building on a Linux or Mac OS X computer.

## BUILDING THE SDK

Run the following command after having setup the build prerequisites:
    $ ./prepare.py bb10-arm
    $ make -C WORK/bb10-arm/cmake

You can add -l to prepare.py script to build using latest sources (might be unstable).
You also can add -d to prepary.py script to build a debug sdk.

If everything is succesful (and after a few minutes) you will find the SDK 
folder in the OUTPUT directory.
It contains the header files, the built libraries and the resource files 
for ARM or i486 (simulator) architectures needed to build a project based on 
linphone.

## OTHER COMMANDS

You can clean the build tree (without deleting the downloaded source code) by 
running:

    $ ./prepare.py -c bb10-arm

You can clean everything (including the downloaded source code) by running:

    $ ./prepare.py -C bb10-arm
