#!/bin/bash

############################################################################
# build.sh
# Copyright (C) 2014  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

pushd `dirname $0` > /dev/null
SCRIPTPATH=`pwd -P`
popd > /dev/null
ROOTPATH=${SCRIPTPATH}/../
LIBLINPHONESDKPATH=${ROOTPATH}/../liblinphonesdk

source toolchains/${TOOLCHAIN}.site

export HOST=$HOST

export CC=$CC
export CXX=$CXX
export LD=$LD
export AR=$AR
export RANLIB=$RANLIB
export STRIP=$STRIP
export NM=$NM

export CPPFLAGS=$CPPFLAGS
export CFLAGS=$CFLAGS
export LDFLAGS=$LDFLAGS

mkdir -p ${LIBLINPHONESDKPATH} && \
mkdir -p ${ROOTPATH}/build-${TOOLCHAIN} && \
cd ${ROOTPATH}/build-${TOOLCHAIN} && \
cmake -DCMAKE_TOOLCHAIN_FILE=${ROOTPATH}/toolchains/${TOOLCHAIN}.cmake -DAUTOTOOLS_CONFIG_SITE=${ROOTPATH}/toolchains/${TOOLCHAIN}.site .. -DCMAKE_INSTALL_PREFIX=${LIBLINPHONESDKPATH}/${TOOLCHAIN} && \
make
