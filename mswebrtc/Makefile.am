EXTRA_DIST=autogen.sh msisac.iss.in 
isac_VERSION = 1.0.0
isac_DIR=$(top_builddir)/webrtc

ISAC_FLAVOUR ?= fix # use 'main' or 'fix'
ISAC_SRC_DIR = webrtc/modules/audio_coding/codecs/isac/$(ISAC_FLAVOUR)
ISAC_COMMON_DIR = webrtc/common_audio/signal_processing

pluginsdir = $(libdir)/mediastreamer/plugins

plugins_LTLIBRARIES=libmsisac.la

# Build the list of isac source files
ISAC_BASE_SRCS = $(ISAC_SRC_DIR)/source/fft.c \
	$(ISAC_SRC_DIR)/source/arith_routines.c \
	$(ISAC_SRC_DIR)/source/arith_routines_hist.c \
	$(ISAC_SRC_DIR)/source/arith_routines_logist.c \
	$(ISAC_SRC_DIR)/source/bandwidth_estimator.c \
	$(ISAC_SRC_DIR)/source/decode.c \
	$(ISAC_SRC_DIR)/source/decode_bwe.c \
	$(ISAC_SRC_DIR)/source/encode.c \
	$(ISAC_SRC_DIR)/source/entropy_coding.c \
	$(ISAC_SRC_DIR)/source/filterbank_tables.c \
	$(ISAC_SRC_DIR)/source/filterbanks.c \
	$(ISAC_SRC_DIR)/source/lattice.c \
	$(ISAC_SRC_DIR)/source/lpc_tables.c \
	$(ISAC_SRC_DIR)/source/pitch_estimator.c \
	$(ISAC_SRC_DIR)/source/pitch_filter.c \
	$(ISAC_SRC_DIR)/source/pitch_gain_tables.c \
	$(ISAC_SRC_DIR)/source/pitch_lag_tables.c \
	$(ISAC_SRC_DIR)/source/spectrum_ar_model_tables.c \
	$(ISAC_SRC_DIR)/source/transform.c

# some files are implementation specific

if ISAC_FLAVOUR_MAIN

# floating point implementation
ISAC_BASE_SRCS += $(ISAC_SRC_DIR)/source/intialize.c \
	$(ISAC_SRC_DIR)/source/crc.c \
	$(ISAC_SRC_DIR)/source/encode_lpc_swb.c \
	$(ISAC_SRC_DIR)/source/filter_functions.c \
	$(ISAC_SRC_DIR)/source/isac.c \
	$(ISAC_SRC_DIR)/source/lpc_analysis.c \
	$(ISAC_SRC_DIR)/source/lpc_gain_swb_tables.c \
	$(ISAC_SRC_DIR)/source/lpc_shape_swb12_tables.c \
	$(ISAC_SRC_DIR)/source/lpc_shape_swb16_tables.c \
	$(ISAC_SRC_DIR)/util/utility.c 

else

# fixed point implementation
ISAC_BASE_SRCS += $(ISAC_SRC_DIR)/source/decode_plc.c \
	$(ISAC_SRC_DIR)/source/filters.c \
	$(ISAC_SRC_DIR)/source/initialize.c \
	$(ISAC_SRC_DIR)/source/isacfix.c \
	$(ISAC_SRC_DIR)/source/lattice_c.c \
	$(ISAC_SRC_DIR)/source/pitch_filter_c.c \
	$(ISAC_SRC_DIR)/source/lpc_masking_model.c \
	$(ISAC_COMMON_DIR)/spl_sqrt.c \
	$(ISAC_COMMON_DIR)/real_fft.c \
	$(ISAC_COMMON_DIR)/filter_ar_fast_q12.c \
	$(ISAC_COMMON_DIR)/webrtc_fft_t_1024_8.c \
	$(ISAC_COMMON_DIR)/complex_bit_reverse.c \
	$(ISAC_COMMON_DIR)/spl_init.c \
	$(ISAC_COMMON_DIR)/splitting_filter.c \
	$(ISAC_COMMON_DIR)/spl_sqrt_floor.c \
	$(ISAC_COMMON_DIR)/levinson_durbin.c \
	$(ISAC_COMMON_DIR)/filter_ar.c \
	$(ISAC_COMMON_DIR)/get_scaling_square.c \
	$(ISAC_COMMON_DIR)/resample_fractional.c \
	$(ISAC_COMMON_DIR)/resample.c \
	$(ISAC_COMMON_DIR)/downsample_fast.c \
	$(ISAC_COMMON_DIR)/filter_ma_fast_q12.c \
	$(ISAC_COMMON_DIR)/auto_corr_to_refl_coef.c \
	$(ISAC_COMMON_DIR)/auto_correlation.c \
	$(ISAC_COMMON_DIR)/refl_coef_to_lpc.c \
	$(ISAC_COMMON_DIR)/dot_product_with_scale.c \
	$(ISAC_COMMON_DIR)/copy_set_operations.c \
	$(ISAC_COMMON_DIR)/spl_version.c \
	$(ISAC_COMMON_DIR)/cross_correlation.c \
	$(ISAC_COMMON_DIR)/sqrt_of_one_minus_x_squared.c \
	$(ISAC_COMMON_DIR)/ilbc_specific_functions.c \
	$(ISAC_COMMON_DIR)/resample_by_2.c \
	$(ISAC_COMMON_DIR)/complex_fft.c \
	$(ISAC_COMMON_DIR)/resample_48khz.c \
	$(ISAC_COMMON_DIR)/energy.c \
	$(ISAC_COMMON_DIR)/get_hanning_window.c \
	$(ISAC_COMMON_DIR)/randomization_functions.c \
	$(ISAC_COMMON_DIR)/vector_scaling_operations.c \
	$(ISAC_COMMON_DIR)/webrtc_fft_t_rad.c \
	$(ISAC_COMMON_DIR)/resample_by_2_internal.c \
	$(ISAC_COMMON_DIR)/lpc_to_refl_coef.c \
	$(ISAC_COMMON_DIR)/min_max_operations.c \
	$(ISAC_COMMON_DIR)/division_operations.c 

# TODO: use assembly-optimized versions when possible
#	$(ISAC_SRC_DIR)/source/filters_neon.c \
#	$(ISAC_SRC_DIR)/source/lpc_masking_model_neon.S \
#	$(ISAC_SRC_DIR)/source/pitch_filter_armv6.S \
#	$(ISAC_SRC_DIR)/source/lattice_neon.S \
#	$(ISAC_SRC_DIR)/source/lattice_armv7.S 

endif


libmsisac_la_SOURCES= $(ISAC_BASE_SRCS) isac_dec.c isac_enc.c

webrtc_repo = git://git.linphone.org/webrtc

BUILT_SOURCES = $(isac_DIR)
GIT=$(shell which git)

$(isac_DIR):
	$(GIT) clone $(webrtc_repo)

extract-sources: $(isac_DIR)


libmsisac_la_LIBADD=\
	$(ORTP_LIBS) \
        $(MEDIASTREAMER_LIBS)

libmsisac_la_LDFLAGS=-module -no-undefined

isac_CFLAGS = -I$(isac_DIR) \
               -I$(ISAC_SRC_DIR)/source -I$(ISAC_SRC_DIR)/util -I$(ISAC_SRC_DIR)/interface \
               -I$(ISAC_COMMON_DIR)/include


AM_CFLAGS=      $(isac_CFLAGS) \
                $(MEDIASTREAMER_CFLAGS) \
				$(STRICT_CFLAGS)

INSTALLDIR=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-install
INSTALLDIR_WITH_PREFIX=$(INSTALLDIR)/$(prefix)
ZIPFILE=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-win32-$(VERSION).zip
ZIP_EXCLUDED=include 

ISS_SCRIPT=msisac.iss
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

zip:
	rm -f $(ZIPFILE)
	rm -rf $(INSTALLDIR)
	mkdir -p $(INSTALLDIR)
	make install DESTDIR=$(INSTALLDIR)
	cp -f $(top_srcdir)/README $(INSTALLDIR_WITH_PREFIX)/.
	cp -f $(top_srcdir)/COPYING $(INSTALLDIR_WITH_PREFIX)/.
	cd $(INSTALLDIR)/$(prefix) && rm -rf $(ZIP_EXCLUDED) && \
	zip -r $(ZIPFILE) *

