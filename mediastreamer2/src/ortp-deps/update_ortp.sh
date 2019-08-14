#!/bin/sh
##
## Copyright (c) 2010-2019 Belledonne Communications SARL.
##
## This file is part of mediastreamer2.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program. If not, see <http://www.gnu.org/licenses/>.
##

BASEDIR=$(dirname $0)
GITURL=git://git.linphone.org/ortp.git
TEMPGITDIR=ortp-temp

cd $BASEDIR
rm -rf $TEMPGITDIR
git clone $GITURL $TEMPGITDIR

mkdir -p ortp
cp -f $TEMPGITDIR/include/ortp/b64.h ortp/
cp -f $TEMPGITDIR/include/ortp/logging.h ortp/
cp -f $TEMPGITDIR/include/ortp/payloadtype.h ortp/
cp -f $TEMPGITDIR/include/ortp/port.h ortp/
cp -f $TEMPGITDIR/include/ortp/str_utils.h ortp/
cp -f $TEMPGITDIR/src/ortp-config-win32.h .
cp -f $TEMPGITDIR/src/utils.h .
cp -f $TEMPGITDIR/src/b64.c .
cp -f $TEMPGITDIR/src/logging.c .
cp -f $TEMPGITDIR/src/payloadtype.c .
cp -f $TEMPGITDIR/src/port.c .
cp -f $TEMPGITDIR/src/str_utils.c .

rm -rf $TEMPGITDIR
