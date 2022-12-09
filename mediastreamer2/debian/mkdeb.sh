#!/bin/sh
##
## Copyright (c) 2010-2022 Belledonne Communications SARL.
##
## This file is part of mediastreamer2 
## (see https://gitlab.linphone.org/BC/public/mediastreamer2).
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU Affero General Public License as
## published by the Free Software Foundation, either version 3 of the
## License, or (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU Affero General Public License for more details.
##
## You should have received a copy of the GNU Affero General Public License
## along with this program. If not, see <http://www.gnu.org/licenses/>.
##

deb_cur_version ()
{
	dpkg-parsechangelog -c1 | sed -rne 's,^Version: ([^-]+).*,\1,p' | sed 's/.*://'
}

deb_package ()
{
	dpkg-parsechangelog -c1 | sed -rne 's,^Source: (.*),\1,p' 
}


conf_version ()
{
	grep -r "AC_INIT" configure.ac | head -n1 |cut -d\[ -f2 | sed 's/\].*//'
}

# tag-offsetghash
git_version ()
{
	git describe | awk '{split($0,c,"-"); print c[1]"-"c[2]c[3]}'
}

buildversion ()
{

}

builddeb ()
{
[ ! -f configure ] && ./autogen.sh
[ ! -f Makefile ] && ./configure

PACKAGE=`deb_package`
CUR_VERSION=`deb_cur_version`

if [ -d .git ]
then
	[ ! -x git ] && echo "install git" && exit -1
	NEXT_VERSION=`git_version`
else
	NEXT_VERSION=`conf_version`
fi

grep "`echo $NEXT_VERSION | sed 's/\./\\\./g'`" debian/changelog
if [ $? -gt 0 ]
then
	dch --newversion 1:$NEXT_VERSION -m "New version"
fi

make dist
rm -rf build-debian
mkdir -p build-debian
mv $(distdir).tar.gz build-debian/$(PACKAGE)_$(VERSION).orig.tar.gz
cd build-debian && tar -xvzf $(PACKAGE)_$(VERSION).orig.tar.gz
cd build-debian/$(PACKAGE)-$(VERSION) && DEB_BUILD_OPTIONS="nostrip debug" dpkg-buildpackage -us -uc



}
if [ "Xdeb" = "X$1 ]
then
	rm -rf build-deb
	cp -

fi
