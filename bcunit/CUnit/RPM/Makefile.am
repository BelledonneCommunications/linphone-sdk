WD=`pwd`
RPM_DEFINES=--define "_topdir ${WD}" --define "_prefix @prefix@"
RPMFLAGS=

PACKAGE_TMPDIR=@PACKAGE@
SOURCE_GZIP_FILE=@PACKAGE@-@VERSION@-@RELEASE@.tar.gz
SOURCE_TAR_FILE=@PACKAGE@-@VERSION@-@RELEASE@.tar

RPM_I386=./RPMS/i386/@PACKAGE@-@VERSION@-@RELEASE@.i386.rpm
RPM_SRC=./SRPMS/@PACKAGE@-@VERSION@-@RELEASE@.src.rpm

RPMDIRS=BUILD SOURCES RPMS RPMS/i386 SRPMS

rpm: CUnit.spec
	rm -rf ${RPMDIRS} && \
	mkdir -p ${RPMDIRS} && \
	cp ../${SOURCE_GZIP_FILE} SOURCES && \
	cd SOURCES && \
	gunzip ${SOURCE_GZIP_FILE} && \
	tar xvf ${SOURCE_TAR_FILE} && \
	rm -f ${SOURCE_TAR_FILE} && \
	cd ..  && \
	cp -R SOURCES/${PACKAGE_TMPDIR} BUILD/${PACKAGE_TMPDIR} && \
	rpm ${RPM_DEFINES} ${RPMFLAGS} -ba @PACKAGE@".spec" && \
	cp ${RPM_I386} .. && \
	cp ${RPM_SRC} .. && \
	rm -rf ${RPMDIRS}
