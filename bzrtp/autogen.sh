#!/bin/sh
##
## Copyright (c) 2014-2019 Belledonne Communications SARL.
##
## This file is part of bzrtp.
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

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

THEDIR=`pwd`
cd $srcdir

#AM_VERSION="1.11"
if ! type aclocal-$AM_VERSION 1>/dev/null 2>&1; then
	AUTOMAKE=automake
	ACLOCAL=aclocal
else
	ACLOCAL=aclocal-${AM_VERSION}
	AUTOMAKE=automake-${AM_VERSION}
fi

if test -f /opt/local/bin/glibtoolize ; then
	# darwin
	LIBTOOLIZE=/opt/local/bin/glibtoolize
else
	LIBTOOLIZE=libtoolize
fi
if test -d /opt/local/share/aclocal ; then
	ACLOCAL_ARGS="-I /opt/local/share/aclocal"
fi

if test -d /share/aclocal ; then
	ACLOCAL_ARGS="-I /share/aclocal"
fi

echo "Generating build scripts for BZRtp: ZRTP engine"
set -x
$LIBTOOLIZE --copy --force
$ACLOCAL  $ACLOCAL_ARGS
#autoheader
$AUTOMAKE --force-missing --add-missing --copy
autoconf

cd $THEDIR

