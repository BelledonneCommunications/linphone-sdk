#!/bin/sh

#AM_VERSION="1.10"
if ! type aclocal-$AM_VERSION 1>/dev/null 2>&1; then
	# automake-1.10 (recommended) is not available on Fedora 8
	AUTOMAKE=automake
	ACLOCAL=aclocal
else
	ACLOCAL=aclocal-${AM_VERSION}
	AUTOMAKE=automake-${AM_VERSION}
fi

INTLTOOLIZE=/usr/bin/intltoolize

if test -f /opt/local/bin/intltoolize ; then
INTLTOOLIZE=/opt/local/bin/intltoolize
else
INTLTOOLIZE=/usr/bin/intltoolize
fi


libtoolize="libtoolize"
for lt in glibtoolize libtoolize15 libtoolize14 libtoolize13 ; do
        if test -x /usr/bin/$lt ; then
                libtoolize=$lt ; break
        fi
        if test -x /usr/local/bin/$lt ; then
                libtoolize=$lt ; break
        fi
        if test -x /opt/local/bin/$lt ; then
                libtoolize=$lt ; break
        fi
done

if test -d /usr/local/share/aclocal ; then
	ACLOCAL_ARGS="$ACLOCAL_ARGS -I /usr/local/share/aclocal"
fi

if test -d /share/aclocal ; then
        ACLOCAL_ARGS="$ACLOCAL_ARGS -I /share/aclocal"
fi

echo "Generating build scripts in mediastreamer..."
set -x
$libtoolize --copy --force
$INTLTOOLIZE --copy --force --automake
$ACLOCAL -I m4 $ACLOCAL_ARGS
autoheader
$AUTOMAKE --force-missing --add-missing --copy
autoconf
