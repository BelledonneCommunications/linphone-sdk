
DOXFILES = doxygen.dox filters.dox ht0-buildagraph.dox

EXTRA_DIST = Doxyfile.in $(DOXFILES)

SOURCES=$(DOXFILES) $(top_srcdir)/include/mediastreamer2/*.h $(top_srcdir)/src/*.h

#html doc
if HAVE_DOXYGEN

# docdir & pkgdocdir are not always defined by automake
pkgdocdir=$(docdir)/$(PACKAGE)-$(VERSION)
doc_htmldir=$(pkgdocdir)/html

doc_html_DATA = $(top_builddir)/help/doc/html/html.tar

$(doc_html_DATA): $(top_builddir)/help/doc/html/index.html
	cd $(top_builddir)/help/doc/html/ && tar cf html.tar *

$(top_builddir)/help/doc/html/index.html: $(SOURCES) Doxyfile Makefile.am
	rm -rf doc
	$(DOXYGEN) Doxyfile

install-data-hook:
	cd $(DESTDIR)$(doc_htmldir) && tar xf html.tar && rm -f html.tar

uninstall-hook:
	cd $(DESTDIR)$(doc_htmldir) && rm -f *

endif

clean-local:
	rm -rf doc
