#!/bin/sh

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
