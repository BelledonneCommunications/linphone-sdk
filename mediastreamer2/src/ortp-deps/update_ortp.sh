#!/bin/sh

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
