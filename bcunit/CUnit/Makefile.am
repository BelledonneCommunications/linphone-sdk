SUBDIRS = CUnit Examples Man RPM

rpm:
	rm -rf RPM/BUILD RPM/SOURCES RPM/RPMS RPM/SRPMS && \
	rm -f @PACKAGE@-@VERSION@-@RELEASE@.tar.gz && \
	make zip && \
	cd RPM && \
	make rpm && \
	cd .. && \
	rm -f @PACKAGE@-@VERSION@-@RELEASE@.tar.gz

zip:
	cd .. && \
	rm -f @PACKAGE@-@VERSION@-@RELEASE@.tar @PACKAGE@-@VERSION@-@RELEASE@.tar.gz && \
	tar cvf @PACKAGE@-@VERSION@-@RELEASE@.tar ${PACKAGE}/* && \
	gzip @PACKAGE@-@VERSION@-@RELEASE@.tar && \
	cp @PACKAGE@-@VERSION@-@RELEASE@.tar.gz ./${PACKAGE}/ && \
	cd ${PACKAGE}
