EXTRA_DIST=autogen.sh msamr.iss.in msamr.spec msamr.spec.in
EXTRA_DIST += CMakeLists.txt \
cmake/FindOpenCoreAMRNB.cmake \
cmake/FindOpenCoreAMRWB.cmake \
cmake/FindVoAMRWBEnc.cmake \
config.h.cmake \
src/CMakeLists.txt

SUBDIRS=src

INSTALLDIR=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-install
INSTALLDIR_WITH_PREFIX=$(INSTALLDIR)/$(prefix)
ZIPFILE=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-win32-$(VERSION).zip
ZIP_EXCLUDED=include lib/mediastreamer/plugins/*.la lib/mediastreamer/plugins/*.a

ISS_SCRIPT=msamr.iss
ISS_SCRIPT_PATH=$(shell cd $(top_srcdir) && pwd)/$(ISS_SCRIPT)
#path to Inno Setup 5 compiler
ISCC=ISCC.exe
PACKAGE_WIN32_FILELIST=$(PACKAGE)-win32.filelist

setup.exe: zip
	cd $(INSTALLDIR_WITH_PREFIX) && \
	rm -f $(PACKAGE_WIN32_FILELIST) && \
	for file in `find` ; do \
		if ! test -d $$file ; then \
			echo "Source: $$file; Destdir: {app}\\`dirname $$file`; Flags: ignoreversion" \
			>> $(PACKAGE_WIN32_FILELIST) ;\
		fi \
	done
	cp $(ISS_SCRIPT) $(INSTALLDIR_WITH_PREFIX)/.
	cd $(INSTALLDIR_WITH_PREFIX) && \
	$(ISCC) $(ISS_SCRIPT) 
	mv $(INSTALLDIR_WITH_PREFIX)/Output/setup.exe $(PACKAGE)-$(VERSION)-setup.exe
	rm -rf $(INSTALLDIR_WITH_PREFIX)/Output
	rm -f $(INSTALLDIR_WITH_PREFIX)/$(PACKAGE_WIN32_FILELIST)
	rm -f $(INSTALLDIR_WITH_PREFIX)/$(ISS_SCRIPT)

dll_deps=libopencore-amrnb-0.dll libopencore-amrwb-0.dll libvo-amrwbenc-0.dll

installfiles:
	rm -f $(ZIPFILE)
	rm -rf $(INSTALLDIR)
	mkdir -p $(INSTALLDIR)
	make install DESTDIR=$(INSTALLDIR)
	- for file in $(dll_deps) ; do \
		cp -f $(prefix)/bin/$$file $(INSTALLDIR_WITH_PREFIX)/lib/mediastreamer/plugins/. ;\
	done

zip:	installfiles
	cp -f $(top_srcdir)/README $(INSTALLDIR_WITH_PREFIX)/.
	cp -f $(top_srcdir)/COPYING $(INSTALLDIR_WITH_PREFIX)/.
	cd $(INSTALLDIR)/$(prefix) && rm -rf $(ZIP_EXCLUDED) && \
	zip -r $(ZIPFILE) *

clean-local:
	-rm -rf $(PACKAGE)-install
	-rm -f $(PACKAGE)-win32-$(VERSION).zip


.phony: rpm

rpm:
	$(MAKE) dist
	TAR_OPTIONS=--wildcards rpmbuild -ta --clean --rmsource --rmspec $(PACKAGE)-$(VERSION).tar.gz

