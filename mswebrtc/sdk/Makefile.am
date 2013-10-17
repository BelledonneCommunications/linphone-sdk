isac_dir = $(srcdir)/webrtc

webrtc_repo = git://git.linphone.org/webrtc

BUILT_SOURCES = $(isac_dir)
GIT=$(shell which git)

$(isac_dir):
	$(GIT) clone $(webrtc_repo)

extract-sources: $(isac_dir)


# Call Skype Makefile to build the library
all-local: $(isac_dir)
#	cd $(isac_dir) && $(MAKE) AS="$(AS)" AR="$(AR)" RANLIB="$(RANLIB)" CC="$(CC) $(CFLAGS)" LD="$(LD)" $(ISAC_MAKE_OPTIONS) $(AM_MAKEFLAGS) lib

check-local: $(isac_dir)
#	cd $(isac_dir) && $(MAKE) $(AM_MAKEFLAGS) test

clean-local: $(isac_dir)

install-data-local: $(isac_dir)

distclean-local:
	rm -rf $(isac_dir)
