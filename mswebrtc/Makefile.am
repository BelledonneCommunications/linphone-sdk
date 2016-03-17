EXTRA_DIST=autogen.sh mswebrtc.iss.in CMakeLists.txt config.h.cmake
webrtc_VERSION = 1.0.0
webrtc_DIR=$(top_srcdir)/webrtc

ISAC_FLAVOUR ?= fix # use 'main' or 'fix'
ISAC_SRC_DIR = $(top_srcdir)/webrtc/webrtc/modules/audio_coding/codecs/isac/$(ISAC_FLAVOUR)
AEC_SRC_DIR = $(top_srcdir)/webrtc/webrtc/modules/audio_processing/aecm
WEBRTC_COMMON_DIR = $(top_srcdir)/webrtc/webrtc/common_audio
WEBRTC_COMMON_SP_DIR = $(WEBRTC_COMMON_DIR)/signal_processing

pluginsdir = $(libdir)/mediastreamer/plugins

plugins_LTLIBRARIES=libmswebrtc.la

WEBRTC_COMMON_SRCS = \ 
$(WEBRTC_COMMON_SP_DIR)/auto_correlation.c          $(WEBRTC_COMMON_SP_DIR)/levinson_durbin.c \
$(WEBRTC_COMMON_SP_DIR)/auto_corr_to_refl_coef.c    $(WEBRTC_COMMON_SP_DIR)/lpc_to_refl_coef.c \
$(WEBRTC_COMMON_SP_DIR)/complex_bit_reverse.c       $(WEBRTC_COMMON_SP_DIR)/min_max_operations.c \
$(WEBRTC_COMMON_SP_DIR)/complex_fft.c             \
$(WEBRTC_COMMON_SP_DIR)/randomization_functions.c \
$(WEBRTC_COMMON_SP_DIR)/copy_set_operations.c       $(WEBRTC_COMMON_SP_DIR)/real_fft.c \
$(WEBRTC_COMMON_SP_DIR)/cross_correlation.c         $(WEBRTC_COMMON_SP_DIR)/refl_coef_to_lpc.c \
$(WEBRTC_COMMON_SP_DIR)/resample_48khz.c \
$(WEBRTC_COMMON_SP_DIR)/resample_by_2.c \
$(WEBRTC_COMMON_SP_DIR)/division_operations.c       $(WEBRTC_COMMON_SP_DIR)/resample_by_2_internal.c \
$(WEBRTC_COMMON_SP_DIR)/dot_product_with_scale.c \
$(WEBRTC_COMMON_SP_DIR)/downsample_fast.c           $(WEBRTC_COMMON_SP_DIR)/resample.c \
$(WEBRTC_COMMON_SP_DIR)/resample_fractional.c \
$(WEBRTC_COMMON_SP_DIR)/spl_init.c \
$(WEBRTC_COMMON_SP_DIR)/energy.c                    $(WEBRTC_COMMON_SP_DIR)/splitting_filter.c \
$(WEBRTC_COMMON_SP_DIR)/filter_ar.c                 $(WEBRTC_COMMON_SP_DIR)/spl_sqrt.c \
$(WEBRTC_COMMON_SP_DIR)/filter_ar_fast_q12.c        $(WEBRTC_COMMON_SP_DIR)/spl_sqrt_floor.c \
$(WEBRTC_COMMON_SP_DIR)/filter_ma_fast_q12.c        $(WEBRTC_COMMON_SP_DIR)/sqrt_of_one_minus_x_squared.c \
$(WEBRTC_COMMON_SP_DIR)/get_hanning_window.c        $(WEBRTC_COMMON_SP_DIR)/vector_scaling_operations.c \
$(WEBRTC_COMMON_SP_DIR)/get_scaling_square.c       \
$(WEBRTC_COMMON_SP_DIR)/ilbc_specific_functions.c

WEBRTC_COMMON_SRCS += $(WEBRTC_COMMON_DIR)/fft4g.c \
	$(WEBRTC_COMMON_DIR)/ring_buffer.c 

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
	$(ISAC_SRC_DIR)/source/transform.c \
	$(ISAC_SRC_DIR)/source/transform_tables.c \
	$(ISAC_SRC_DIR)/source/pitch_estimator_c.c

# some files are implementation specific

if ISAC_FLAVOUR_MAIN

# floating point implementation
ISAC_BASE_SRCS += $(ISAC_SRC_DIR)/source/initialize.c \
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
	$(ISAC_SRC_DIR)/source/lpc_masking_model.c

# TODO: use assembly-optimized versions when possible
#	$(ISAC_SRC_DIR)/source/filters_neon.c \
#	$(ISAC_SRC_DIR)/source/lpc_masking_model_neon.S \
#	$(ISAC_SRC_DIR)/source/pitch_filter_armv6.S \
#	$(ISAC_SRC_DIR)/source/lattice_neon.S \
#	$(ISAC_SRC_DIR)/source/lattice_armv7.S

endif

AEC_BASE_SRCS = \
	$(AEC_SRC_DIR)/echo_control_mobile.c \
	$(AEC_SRC_DIR)/aecm_core.c \
	$(AEC_SRC_DIR)/../utility/delay_estimator.c \
	$(AEC_SRC_DIR)/../utility/delay_estimator_wrapper.c

AEC_BASE_SRCS += $(AEC_SRC_DIR)/aecm_core_c.c

libmswebrtc_la_SOURCES= mswebrtc.c $(WEBRTC_COMMON_SRCS)
if BUILD_ISAC
libmswebrtc_la_SOURCES+= $(ISAC_BASE_SRCS) isac_dec.c isac_enc.c
endif
if BUILD_AEC
libmswebrtc_la_SOURCES+= $(AEC_BASE_SRCS) aec.c
endif


libmswebrtc_la_LIBADD= \
	$(ORTP_LIBS) \
	$(MEDIASTREAMER_LIBS)

libmswebrtc_la_LDFLAGS=-module -no-undefined

webrtc_CFLAGS = \
	-I$(webrtc_DIR) \
	-I$(WEBRTC_COMMON_SP_DIR)/include
if BUILD_ISAC
webrtc_CFLAGS += \
	-I$(ISAC_SRC_DIR)/source \
	-I$(ISAC_SRC_DIR)/util \
	-I$(ISAC_SRC_DIR)/interface \
	-I$(ISAC_SRC_DIR)/include
endif
if BUILD_AEC
webrtc_CFLAGS += \
	-I$(AEC_SRC_DIR)/include \
	-I$(top_srcdir)/webrtc/common_audio/signal_processing/include \
	-I$(top_srcdir)/webrtc/modules/audio_processing/utility \
	-I$(top_srcdir)/webrtc/system_wrappers/interface
endif

webrtc_CFLAGS += -DWEBRTC_POSIX=1

AM_CFLAGS= \
	$(webrtc_CFLAGS) \
	$(MEDIASTREAMER_CFLAGS) \
	$(STRICT_CFLAGS)

INSTALLDIR=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-install
INSTALLDIR_WITH_PREFIX=$(INSTALLDIR)/$(prefix)
ZIPFILE=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-win32-$(VERSION).zip
ZIP_EXCLUDED=include 

ISS_SCRIPT=mswebrtc.iss
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
