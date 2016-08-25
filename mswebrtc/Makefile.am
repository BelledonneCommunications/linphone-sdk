EXTRA_DIST=autogen.sh mswebrtc.iss.in CMakeLists.txt config.h.cmake
webrtc_VERSION = 1.0.0
webrtc_DIR=$(top_srcdir)/webrtc

# use 'main' or 'fix'
ISAC_FLAVOUR = fix
ISAC_SRC_DIR = webrtc/webrtc/modules/audio_coding/codecs/isac/$(ISAC_FLAVOUR)
ILBC_SRC_DIR = webrtc/webrtc/modules/audio_coding/codecs/ilbc
AEC_SRC_DIR = webrtc/webrtc/modules/audio_processing/aecm
WEBRTC_COMMON_DIR = webrtc/webrtc/common_audio
WEBRTC_COMMON_SP_DIR = webrtc/webrtc/common_audio/signal_processing

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
#
## floating point implementation
# August 16th, 2016 : disabling support of floating point
# because of compilation issues
#TODO: Fix floating point build
#ISAC_BASE_SRCS += $(ISAC_SRC_DIR)/source/initialize.c \
#	$(ISAC_SRC_DIR)/source/crc.c \
#	$(ISAC_SRC_DIR)/source/encode_lpc_swb.c \
#	$(ISAC_SRC_DIR)/source/filter_functions.c \
#	$(ISAC_SRC_DIR)/source/isac.c \
#	$(ISAC_SRC_DIR)/source/lpc_analysis.c \
#	$(ISAC_SRC_DIR)/source/lpc_gain_swb_tables.c \
#	$(ISAC_SRC_DIR)/source/lpc_shape_swb12_tables.c \
#	$(ISAC_SRC_DIR)/source/lpc_shape_swb16_tables.c \
#	$(ISAC_SRC_DIR)/util/utility.c
#
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

ILBC_BASE_SRCS = \
	$(ILBC_SRC_DIR)/abs_quant.c \
	$(ILBC_SRC_DIR)/abs_quant_loop.c \
	$(ILBC_SRC_DIR)/augmented_cb_corr.c \
	$(ILBC_SRC_DIR)/bw_expand.c \
	$(ILBC_SRC_DIR)/cb_construct.c \
	$(ILBC_SRC_DIR)/cb_mem_energy_augmentation.c \
	$(ILBC_SRC_DIR)/cb_mem_energy.c \
	$(ILBC_SRC_DIR)/cb_mem_energy_calc.c \
	$(ILBC_SRC_DIR)/cb_search.c \
	$(ILBC_SRC_DIR)/cb_search_core.c \
	$(ILBC_SRC_DIR)/cb_update_best_index.c \
	$(ILBC_SRC_DIR)/chebyshev.c \
	$(ILBC_SRC_DIR)/comp_corr.c \
	$(ILBC_SRC_DIR)/constants.c \
	$(ILBC_SRC_DIR)/create_augmented_vec.c \
	$(ILBC_SRC_DIR)/decode.c \
	$(ILBC_SRC_DIR)/decode_residual.c \
	$(ILBC_SRC_DIR)/decoder_interpolate_lsf.c \
	$(ILBC_SRC_DIR)/do_plc.c \
	$(ILBC_SRC_DIR)/encode.c \
	$(ILBC_SRC_DIR)/energy_inverse.c \
	$(ILBC_SRC_DIR)/enhancer.c \
	$(ILBC_SRC_DIR)/enhancer_interface.c \
	$(ILBC_SRC_DIR)/enh_upsample.c \
	$(ILBC_SRC_DIR)/filtered_cb_vecs.c \
	$(ILBC_SRC_DIR)/frame_classify.c \
	$(ILBC_SRC_DIR)/gain_dequant.c \
	$(ILBC_SRC_DIR)/gain_quant.c \
	$(ILBC_SRC_DIR)/get_cd_vec.c \
	$(ILBC_SRC_DIR)/get_lsp_poly.c \
	$(ILBC_SRC_DIR)/get_sync_seq.c \
	$(ILBC_SRC_DIR)/hp_input.c \
	$(ILBC_SRC_DIR)/hp_output.c \
	$(ILBC_SRC_DIR)/ilbc.c \
	$(ILBC_SRC_DIR)/index_conv_dec.c \
	$(ILBC_SRC_DIR)/index_conv_enc.c \
	$(ILBC_SRC_DIR)/init_decode.c \
	$(ILBC_SRC_DIR)/init_encode.c \
	$(ILBC_SRC_DIR)/interpolate.c \
	$(ILBC_SRC_DIR)/interpolate_samples.c \
	$(ILBC_SRC_DIR)/lpc_encode.c \
	$(ILBC_SRC_DIR)/lsf_check.c \
	$(ILBC_SRC_DIR)/lsf_interpolate_to_poly_dec.c \
	$(ILBC_SRC_DIR)/lsf_interpolate_to_poly_enc.c \
	$(ILBC_SRC_DIR)/lsf_to_lsp.c \
	$(ILBC_SRC_DIR)/lsf_to_poly.c \
	$(ILBC_SRC_DIR)/lsp_to_lsf.c \
	$(ILBC_SRC_DIR)/my_corr.c \
	$(ILBC_SRC_DIR)/nearest_neighbor.c \
	$(ILBC_SRC_DIR)/pack_bits.c \
	$(ILBC_SRC_DIR)/poly_to_lsf.c \
	$(ILBC_SRC_DIR)/poly_to_lsp.c \
	$(ILBC_SRC_DIR)/refiner.c \
	$(ILBC_SRC_DIR)/simple_interpolate_lsf.c \
	$(ILBC_SRC_DIR)/simple_lpc_analysis.c \
	$(ILBC_SRC_DIR)/simple_lsf_dequant.c \
	$(ILBC_SRC_DIR)/simple_lsf_quant.c \
	$(ILBC_SRC_DIR)/smooth.c \
	$(ILBC_SRC_DIR)/smooth_out_data.c \
	$(ILBC_SRC_DIR)/sort_sq.c \
	$(ILBC_SRC_DIR)/split_vq.c \
	$(ILBC_SRC_DIR)/state_construct.c \
	$(ILBC_SRC_DIR)/state_search.c \
	$(ILBC_SRC_DIR)/swap_bytes.c \
	$(ILBC_SRC_DIR)/unpack_bits.c \
	$(ILBC_SRC_DIR)/vq3.c \
	$(ILBC_SRC_DIR)/vq4.c \
	$(ILBC_SRC_DIR)/window32_w32.c \
	$(ILBC_SRC_DIR)/xcorr_coef.c 

libmswebrtc_la_SOURCES= mswebrtc.c $(top_srcdir)/$(WEBRTC_COMMON_SRCS)\
	$(top_srcdir)/webrtc/webrtc/common_audio/signal_processing/include/signal_processing_library.h \
	$(top_srcdir)/webrtc/webrtc/common_audio/signal_processing/include/spl_inl.h
if BUILD_ISAC
libmswebrtc_la_SOURCES+= $(top_srcdir)/$(ISAC_BASE_SRCS) isac_dec.c isac_enc.c \
	$(top_srcdir)/isac_constants.h \
	$(top_srcdir)/$(ISAC_SRC_DIR)/include/isacfix.h \
	$(top_srcdir)/webrtc/webrtc/modules/audio_coding/codecs/isac/bandwidth_info.h \
	$(top_srcdir)/webrtc/webrtc/typedefs.h
#$(ISAC_SRC_DIR)/../bandwith_info.h
endif
if BUILD_AEC
libmswebrtc_la_SOURCES+= $(top_srcdir)/$(AEC_BASE_SRCS) aec.c \
	$(top_srcdir)/$(AEC_SRC_DIR)/include/echo_control_mobile.h
endif
if BUILD_ILBC
libmswebrtc_la_SOURCES+= ilbc.c $(top_srcdir)/$(ILBC_BASE_SRCS)\
	$(top_srcdir)/$(ILBC_SRC_DIR)/include/ilbc.h 
endif


libmswebrtc_la_LIBADD= \
	$(ORTP_LIBS) \
	$(MEDIASTREAMER_LIBS)

libmswebrtc_la_LDFLAGS=-module -no-undefined

webrtc_CFLAGS = \
	-I$(webrtc_DIR) \
	-I$(top_srcdir)/$(WEBRTC_COMMON_SP_DIR)/include
if BUILD_ISAC
webrtc_CFLAGS += \
	-I$(top_srcdir)/$(ISAC_SRC_DIR)/source \
	-I$(top_srcdir)/$(ISAC_SRC_DIR)/util \
	-I$(top_srcdir)/$(ISAC_SRC_DIR)/interface \
	-I$(top_srcdir)/$(ISAC_SRC_DIR)/include
endif
if BUILD_AEC
webrtc_CFLAGS += \
	-I$(top_srcdir)/$(AEC_SRC_DIR)/include \
	-I$(top_srcdir)/webrtc/common_audio/signal_processing/include \
	-I$(top_srcdir)/webrtc/modules/audio_processing/utility \
	-I$(top_srcdir)/webrtc/system_wrappers/interface
endif

if BUILD_ILBC
webrtc_CFLAGS += \
	-I$(top_srcdir)/$(ILBC_SRC_DIR)/include 
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
