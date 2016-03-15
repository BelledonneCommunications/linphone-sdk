# FLEXISIP DEB AND RPM FILES #

## BUILD PREREQUISITES

The common prerequisites listed in the README.md file are needed.
Note that building flexisip packages requires to be building on a Linux host.

### Redhat / RPM-based

    sudo yum install @development-tools speex-devel \
        hiredis-devel libtool-ltdl-devel libiodbc-devel
    make build-flexisip-rpm

### Debian

To be able to build Flexisip debs on a Debian machine, you will have to install
all of these (plus the usual compilation tools):

    sudo aptitude install cmake rpm libmysqlclient-dev libmysql++-dev \
    	libmysqld-dev libssl-dev bison doxygen
    sudo aptitude install alien fakeroot libhiredis-dev
    make build-flexisip
