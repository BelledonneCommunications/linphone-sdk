SUBDIRS = CUnit Examples Man

distdir=${PACKAGE}-${VERSION}-${RELEASE}

WD=`pwd`
RPM_DEFINES=--define "_topdir ${WD}" --define "_prefix @prefix@"
RPM_BUILD_ROOT=${PWD}
RPMFLAGS=
ZIP=zip
RPM=rpm

PACKAGE_TMPSPEC=${TMPDIR}/@PACKAGE@-@VERSION@-@RELEASE@.spec
SPECFILE=@PACKAGE@".spec"
CHANGELOG="ChangeLog"
SOURCE_GZIP_FILE=@PACKAGE@-@VERSION@-@RELEASE@.tar.gz
SOURCE_ZIP_FILE=@PACKAGE@-@VERSION@-@RELEASE@.zip
SOURCE_TAR_FILE=@PACKAGE@-@VERSION@-@RELEASE@.tar

RPM_ATHLON=./RPMS/athlon/@PACKAGE@-@VERSION@-@RELEASE@.athlon.rpm
RPM_I386=./RPMS/i386/@PACKAGE@-@VERSION@-@RELEASE@.i386.rpm
RPM_I486=./RPMS/i486/@PACKAGE@-@VERSION@-@RELEASE@.i486.rpm
RPM_I586=./RPMS/i586/@PACKAGE@-@VERSION@-@RELEASE@.i586.rpm
RPM_I686=./RPMS/i686/@PACKAGE@-@VERSION@-@RELEASE@.i686.rpm
RPM_K6=./RPMS/athlon/@PACKAGE@-@VERSION@-@RELEASE@.k6.rpm
RPM_SRC=./SRPMS/@PACKAGE@-@VERSION@-@RELEASE@.src.rpm

RPMDIRS=BUILD SOURCES RPMS RPMS/i386 SRPMS

clean-all: clean
	rm -f ${SOURCE_GZIP_FILE} ${SOURCE_ZIP_FILE} ${SOURCE_TAR_FILE} *.rpm
	rm -rf ${RPMDIRS}

distclean-all: distclean
	rm -f ${SOURCE_GZIP_FILE} ${SOURCE_ZIP_FILE} ${SOURCE_TAR_FILE} *.rpm
	rm -rf ${RPMDIRS}

rpm: rpm-package

zip: clean
	cd .. && \
	rm -f ${SOURCE_ZIP_FILE} && \
	${ZIP} -rT ${SOURCE_ZIP_FILE} @PACKAGE@-@VERSION@-@RELEASE@/* && \
	cp -f ${SOURCE_ZIP_FILE} ./@PACKAGE@-@VERSION@-@RELEASE@/ && \
	cd @PACKAGE@-@VERSION@-@RELEASE@

tarball: clean
	set -x
	cd .. && \
	rm -f ${SOURCE_TAR_FILE} && \
	tar cvf ${SOURCE_TAR_FILE} @PACKAGE@-@VERSION@-@RELEASE@/* && \
	cp -f ${SOURCE_TAR_FILE} ./@PACKAGE@-@VERSION@-@RELEASE@/  && \
	cd @PACKAGE@-@VERSION@-@RELEASE@

gzip: clean tarball
	rm -f ${SOURCE_GZIP_FILE} && \
	gzip ${SOURCE_TAR_FILE}

rpm-package: 
	cp ${SPECFILE} ${PACKAGE_TMPSPEC} && \
	echo "%changelog" >> ${PACKAGE_TMPSPEC} && \
	cat ${CHANGELOG} >> ${PACKAGE_TMPSPEC} && \
	rm -rf ${RPMDIRS} && \
	mkdir -p ${RPMDIRS} && \
	export RPM_BUILD_ROOT=${PWD} && \
	${RPM} ${RPM_DEFINES} ${RPMFLAGS} -bb ${PACKAGE_TMPSPEC} && \
	( (cp -f ${RPM_ATHLON} .) || (cp -f ${RPM_I386} .) || (cp -f ${RPM_I486} .) \
	  || (cp -f ${RPM_I586} .) || (cp -f ${RPM_I686} .) \
	  || (cp -f ${RPM_K6} .) || (cp -f ${RPM_NOARCH} .) ) && \
	rm -rf ${RPMDIRS} 
