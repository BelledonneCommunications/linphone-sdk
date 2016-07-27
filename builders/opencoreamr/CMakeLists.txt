############################################################################
# CMakeLists.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

cmake_minimum_required(VERSION 3.0)
project(OPENCOREAMR)


option(ENABLE_STATIC "Build static library (default is shared library)." NO)
option(ENABLE_AMRNB_ENCODER "Enable AMR-NB encoder (default is yes)." YES)
option(ENABLE_AMRNB_DECODER "Enable AMR-NB decoder (default is yes)." YES)
option(ENABLE_AMRWB_DECODER "Enable AMR-WB decoder (default is yes)." YES)


set(AMRNB_COMMON_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/opencore/codecs_v2/audio/gsm_amr/amr_nb/common/src)
set(AMRNB_DEC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/src)
set(AMRNB_ENC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/opencore/codecs_v2/audio/gsm_amr/amr_nb/enc/src)
set(AMRNB_INCLUDE_DIRS
	oscl
	${AMRNB_DEC_SRC_DIR}
	opencore/codecs_v2/audio/gsm_amr/amr_nb/common/include
	opencore/codecs_v2/audio/gsm_amr/amr_nb/dec/include
	opencore/codecs_v2/audio/gsm_amr/common/dec/include
	${AMRNB_ENC_SRC_DIR}
)


set(AMRNB_SOURCES
	amrnb/wrapper.cpp
	${AMRNB_COMMON_SRC_DIR}/add.cpp
	${AMRNB_COMMON_SRC_DIR}/az_lsp.cpp
	${AMRNB_COMMON_SRC_DIR}/bitno_tab.cpp
	${AMRNB_COMMON_SRC_DIR}/bitreorder_tab.cpp
	${AMRNB_COMMON_SRC_DIR}/c2_9pf_tab.cpp
	${AMRNB_COMMON_SRC_DIR}/div_s.cpp
	${AMRNB_COMMON_SRC_DIR}/extract_h.cpp
	${AMRNB_COMMON_SRC_DIR}/extract_l.cpp
	${AMRNB_COMMON_SRC_DIR}/gains_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/gc_pred.cpp
	${AMRNB_COMMON_SRC_DIR}/get_const_tbls.cpp
	${AMRNB_COMMON_SRC_DIR}/gmed_n.cpp
	${AMRNB_COMMON_SRC_DIR}/gray_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/grid_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/int_lpc.cpp
	${AMRNB_COMMON_SRC_DIR}/inv_sqrt.cpp
	${AMRNB_COMMON_SRC_DIR}/inv_sqrt_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/l_deposit_h.cpp
	${AMRNB_COMMON_SRC_DIR}/l_deposit_l.cpp
	${AMRNB_COMMON_SRC_DIR}/log2.cpp
	${AMRNB_COMMON_SRC_DIR}/log2_norm.cpp
	${AMRNB_COMMON_SRC_DIR}/log2_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/lsfwt.cpp
	${AMRNB_COMMON_SRC_DIR}/l_shr_r.cpp
	${AMRNB_COMMON_SRC_DIR}/lsp_az.cpp
	${AMRNB_COMMON_SRC_DIR}/lsp.cpp
	${AMRNB_COMMON_SRC_DIR}/lsp_lsf.cpp
	${AMRNB_COMMON_SRC_DIR}/lsp_lsf_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/lsp_tab.cpp
	${AMRNB_COMMON_SRC_DIR}/mult_r.cpp
	${AMRNB_COMMON_SRC_DIR}/negate.cpp
	${AMRNB_COMMON_SRC_DIR}/norm_l.cpp
	${AMRNB_COMMON_SRC_DIR}/norm_s.cpp
	${AMRNB_COMMON_SRC_DIR}/overflow_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/ph_disp_tab.cpp
	${AMRNB_COMMON_SRC_DIR}/pow2.cpp
	${AMRNB_COMMON_SRC_DIR}/pow2_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/pred_lt.cpp
	${AMRNB_COMMON_SRC_DIR}/q_plsf_3.cpp
	${AMRNB_COMMON_SRC_DIR}/q_plsf_3_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/q_plsf_5.cpp
	${AMRNB_COMMON_SRC_DIR}/q_plsf_5_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/q_plsf.cpp
	${AMRNB_COMMON_SRC_DIR}/qua_gain_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/reorder.cpp
	${AMRNB_COMMON_SRC_DIR}/residu.cpp
	${AMRNB_COMMON_SRC_DIR}/round.cpp
	${AMRNB_COMMON_SRC_DIR}/set_zero.cpp
	${AMRNB_COMMON_SRC_DIR}/shr.cpp
	${AMRNB_COMMON_SRC_DIR}/shr_r.cpp
	${AMRNB_COMMON_SRC_DIR}/sqrt_l.cpp
	${AMRNB_COMMON_SRC_DIR}/sqrt_l_tbl.cpp
	${AMRNB_COMMON_SRC_DIR}/sub.cpp
	${AMRNB_COMMON_SRC_DIR}/syn_filt.cpp
	${AMRNB_COMMON_SRC_DIR}/weight_a.cpp
	${AMRNB_COMMON_SRC_DIR}/window_tab.cpp
)
if(ENABLE_AMRNB_DECODER)
	list(APPEND AMRNB_SOURCES
		${AMRNB_DEC_SRC_DIR}/agc.cpp
		${AMRNB_DEC_SRC_DIR}/amrdecode.cpp
		${AMRNB_DEC_SRC_DIR}/a_refl.cpp
		${AMRNB_DEC_SRC_DIR}/b_cn_cod.cpp
		${AMRNB_DEC_SRC_DIR}/bgnscd.cpp
		${AMRNB_DEC_SRC_DIR}/c_g_aver.cpp
		${AMRNB_DEC_SRC_DIR}/d1035pf.cpp
		${AMRNB_DEC_SRC_DIR}/d2_11pf.cpp
		${AMRNB_DEC_SRC_DIR}/d2_9pf.cpp
		${AMRNB_DEC_SRC_DIR}/d3_14pf.cpp
		${AMRNB_DEC_SRC_DIR}/d4_17pf.cpp
		${AMRNB_DEC_SRC_DIR}/d8_31pf.cpp
		${AMRNB_DEC_SRC_DIR}/dec_amr.cpp
		${AMRNB_DEC_SRC_DIR}/dec_gain.cpp
		${AMRNB_DEC_SRC_DIR}/dec_input_format_tab.cpp
		${AMRNB_DEC_SRC_DIR}/dec_lag3.cpp
		${AMRNB_DEC_SRC_DIR}/dec_lag6.cpp
		${AMRNB_DEC_SRC_DIR}/d_gain_c.cpp
		${AMRNB_DEC_SRC_DIR}/d_gain_p.cpp
		${AMRNB_DEC_SRC_DIR}/d_plsf_3.cpp
		${AMRNB_DEC_SRC_DIR}/d_plsf_5.cpp
		${AMRNB_DEC_SRC_DIR}/d_plsf.cpp
		${AMRNB_DEC_SRC_DIR}/dtx_dec.cpp
		${AMRNB_DEC_SRC_DIR}/ec_gains.cpp
		${AMRNB_DEC_SRC_DIR}/ex_ctrl.cpp
		${AMRNB_DEC_SRC_DIR}/if2_to_ets.cpp
		${AMRNB_DEC_SRC_DIR}/int_lsf.cpp
		${AMRNB_DEC_SRC_DIR}/lsp_avg.cpp
		${AMRNB_DEC_SRC_DIR}/ph_disp.cpp
		${AMRNB_DEC_SRC_DIR}/post_pro.cpp
		${AMRNB_DEC_SRC_DIR}/preemph.cpp
		${AMRNB_DEC_SRC_DIR}/pstfilt.cpp
		${AMRNB_DEC_SRC_DIR}/qgain475_tab.cpp
		${AMRNB_DEC_SRC_DIR}/sp_dec.cpp
		${AMRNB_DEC_SRC_DIR}/wmf_to_ets.cpp
	)
endif()
if(ENABLE_AMRNB_ENCODER)
	list(APPEND AMRNB_SOURCES
		${AMRNB_ENC_SRC_DIR}/amrencode.cpp
		${AMRNB_ENC_SRC_DIR}/autocorr.cpp
		${AMRNB_ENC_SRC_DIR}/c1035pf.cpp
		${AMRNB_ENC_SRC_DIR}/c2_11pf.cpp
		${AMRNB_ENC_SRC_DIR}/c2_9pf.cpp
		${AMRNB_ENC_SRC_DIR}/c3_14pf.cpp
		${AMRNB_ENC_SRC_DIR}/c4_17pf.cpp
		${AMRNB_ENC_SRC_DIR}/c8_31pf.cpp
		${AMRNB_ENC_SRC_DIR}/calc_cor.cpp
		${AMRNB_ENC_SRC_DIR}/calc_en.cpp
		${AMRNB_ENC_SRC_DIR}/cbsearch.cpp
		${AMRNB_ENC_SRC_DIR}/cl_ltp.cpp
		${AMRNB_ENC_SRC_DIR}/cod_amr.cpp
		${AMRNB_ENC_SRC_DIR}/convolve.cpp
		${AMRNB_ENC_SRC_DIR}/cor_h.cpp
		${AMRNB_ENC_SRC_DIR}/cor_h_x2.cpp
		${AMRNB_ENC_SRC_DIR}/cor_h_x.cpp
		${AMRNB_ENC_SRC_DIR}/corrwght_tab.cpp
		${AMRNB_ENC_SRC_DIR}/div_32.cpp
		${AMRNB_ENC_SRC_DIR}/dtx_enc.cpp
		${AMRNB_ENC_SRC_DIR}/enc_lag3.cpp
		${AMRNB_ENC_SRC_DIR}/enc_lag6.cpp
		${AMRNB_ENC_SRC_DIR}/enc_output_format_tab.cpp
		${AMRNB_ENC_SRC_DIR}/ets_to_if2.cpp
		${AMRNB_ENC_SRC_DIR}/ets_to_wmf.cpp
		${AMRNB_ENC_SRC_DIR}/g_adapt.cpp
		${AMRNB_ENC_SRC_DIR}/gain_q.cpp
		${AMRNB_ENC_SRC_DIR}/g_code.cpp
		${AMRNB_ENC_SRC_DIR}/g_pitch.cpp
		${AMRNB_ENC_SRC_DIR}/hp_max.cpp
		${AMRNB_ENC_SRC_DIR}/inter_36.cpp
		${AMRNB_ENC_SRC_DIR}/inter_36_tab.cpp
		${AMRNB_ENC_SRC_DIR}/l_abs.cpp
		${AMRNB_ENC_SRC_DIR}/lag_wind.cpp
		${AMRNB_ENC_SRC_DIR}/lag_wind_tab.cpp
		${AMRNB_ENC_SRC_DIR}/l_comp.cpp
		${AMRNB_ENC_SRC_DIR}/levinson.cpp
		${AMRNB_ENC_SRC_DIR}/l_extract.cpp
		${AMRNB_ENC_SRC_DIR}/lflg_upd.cpp
		${AMRNB_ENC_SRC_DIR}/l_negate.cpp
		${AMRNB_ENC_SRC_DIR}/lpc.cpp
		${AMRNB_ENC_SRC_DIR}/ol_ltp.cpp
		${AMRNB_ENC_SRC_DIR}/pitch_fr.cpp
		${AMRNB_ENC_SRC_DIR}/pitch_ol.cpp
		${AMRNB_ENC_SRC_DIR}/p_ol_wgh.cpp
		${AMRNB_ENC_SRC_DIR}/pre_big.cpp
		${AMRNB_ENC_SRC_DIR}/pre_proc.cpp
		${AMRNB_ENC_SRC_DIR}/prm2bits.cpp
		${AMRNB_ENC_SRC_DIR}/qgain475.cpp
		${AMRNB_ENC_SRC_DIR}/qgain795.cpp
		${AMRNB_ENC_SRC_DIR}/q_gain_c.cpp
		${AMRNB_ENC_SRC_DIR}/q_gain_p.cpp
		${AMRNB_ENC_SRC_DIR}/qua_gain.cpp
		${AMRNB_ENC_SRC_DIR}/s10_8pf.cpp
		${AMRNB_ENC_SRC_DIR}/set_sign.cpp
		${AMRNB_ENC_SRC_DIR}/sid_sync.cpp
		${AMRNB_ENC_SRC_DIR}/sp_enc.cpp
		${AMRNB_ENC_SRC_DIR}/spreproc.cpp
		${AMRNB_ENC_SRC_DIR}/spstproc.cpp
		${AMRNB_ENC_SRC_DIR}/ton_stab.cpp
		${AMRNB_ENC_SRC_DIR}/vad1.cpp
	)
endif()
if(WIN32)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/amrnb/opencore-amrnb.sym AMRNB_SYM)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/opencore-amrnb.def "LIBRARY opencore-amrnb\nEXPORTS\n${AMRNB_SYM}")
	list(APPEND AMRNB_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/opencore-amrnb.def)
endif()


if(ENABLE_STATIC)
	add_library(opencore-amrnb STATIC ${AMRNB_SOURCES})
else()
	add_library(opencore-amrnb SHARED ${AMRNB_SOURCES})
	set_target_properties(opencore-amrnb PROPERTIES VERSION 0.0.3)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/opencore-amrnb.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()
target_include_directories(opencore-amrnb PRIVATE ${AMRNB_INCLUDE_DIRS})

install(TARGETS opencore-amrnb
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

set(AMRNB_HEADER_FILES
	amrnb/interf_dec.h
	amrnb/interf_enc.h
)

install(FILES ${AMRNB_HEADER_FILES}
	DESTINATION include/opencore-amrnb
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)



if(ENABLE_AMRWB_DECODER)
	set(AMRWB_DEC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/src)
	set(AMRWB_INCLUDE_DIRS
		oscl
		${AMRWB_DEC_SRC_DIR}
		opencore/codecs_v2/audio/gsm_amr/amr_wb/dec/include
		opencore/codecs_v2/audio/gsm_amr/common/dec/include
	)

	set(AMRWB_SOURCES
		amrwb/wrapper.cpp
		${AMRWB_DEC_SRC_DIR}/agc2_amr_wb.cpp
		${AMRWB_DEC_SRC_DIR}/band_pass_6k_7k.cpp
		${AMRWB_DEC_SRC_DIR}/dec_acelp_2p_in_64.cpp
		${AMRWB_DEC_SRC_DIR}/dec_acelp_4p_in_64.cpp
		${AMRWB_DEC_SRC_DIR}/dec_alg_codebook.cpp
		${AMRWB_DEC_SRC_DIR}/dec_gain2_amr_wb.cpp
		${AMRWB_DEC_SRC_DIR}/deemphasis_32.cpp
		${AMRWB_DEC_SRC_DIR}/dtx_decoder_amr_wb.cpp
		${AMRWB_DEC_SRC_DIR}/get_amr_wb_bits.cpp
		${AMRWB_DEC_SRC_DIR}/highpass_400hz_at_12k8.cpp
		${AMRWB_DEC_SRC_DIR}/highpass_50hz_at_12k8.cpp
		${AMRWB_DEC_SRC_DIR}/homing_amr_wb_dec.cpp
		${AMRWB_DEC_SRC_DIR}/interpolate_isp.cpp
		${AMRWB_DEC_SRC_DIR}/isf_extrapolation.cpp
		${AMRWB_DEC_SRC_DIR}/isp_az.cpp
		${AMRWB_DEC_SRC_DIR}/isp_isf.cpp
		${AMRWB_DEC_SRC_DIR}/lagconceal.cpp
		${AMRWB_DEC_SRC_DIR}/low_pass_filt_7k.cpp
		${AMRWB_DEC_SRC_DIR}/median5.cpp
		${AMRWB_DEC_SRC_DIR}/mime_io.cpp
		${AMRWB_DEC_SRC_DIR}/noise_gen_amrwb.cpp
		${AMRWB_DEC_SRC_DIR}/normalize_amr_wb.cpp
		${AMRWB_DEC_SRC_DIR}/oversamp_12k8_to_16k.cpp
		${AMRWB_DEC_SRC_DIR}/phase_dispersion.cpp
		${AMRWB_DEC_SRC_DIR}/pit_shrp.cpp
		${AMRWB_DEC_SRC_DIR}/pred_lt4.cpp
		${AMRWB_DEC_SRC_DIR}/preemph_amrwb_dec.cpp
		${AMRWB_DEC_SRC_DIR}/pvamrwbdecoder.cpp
		${AMRWB_DEC_SRC_DIR}/pvamrwb_math_op.cpp
		${AMRWB_DEC_SRC_DIR}/q_gain2_tab.cpp
		${AMRWB_DEC_SRC_DIR}/qisf_ns.cpp
		${AMRWB_DEC_SRC_DIR}/qisf_ns_tab.cpp
		${AMRWB_DEC_SRC_DIR}/qpisf_2s.cpp
		${AMRWB_DEC_SRC_DIR}/qpisf_2s_tab.cpp
		${AMRWB_DEC_SRC_DIR}/scale_signal.cpp
		${AMRWB_DEC_SRC_DIR}/synthesis_amr_wb.cpp
		${AMRWB_DEC_SRC_DIR}/voice_factor.cpp
		${AMRWB_DEC_SRC_DIR}/wb_syn_filt.cpp
		${AMRWB_DEC_SRC_DIR}/weight_amrwb_lpc.cpp
	)
	if(WIN32)
		file(READ ${CMAKE_CURRENT_SOURCE_DIR}/amrwb/opencore-amrwb.sym AMRWB_SYM)
		file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/opencore-amrwb.def "LIBRARY opencore-amrwb\nEXPORTS\n${AMRWB_SYM}")
		list(APPEND AMRWB_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/opencore-amrwb.def)
	endif()

	if(ENABLE_STATIC)
		add_library(opencore-amrwb STATIC ${AMRWB_SOURCES})
	else()
		add_library(opencore-amrwb SHARED ${AMRWB_SOURCES})
		set_target_properties(opencore-amrwb PROPERTIES VERSION 0.0.3)
		if(MSVC)
			if(CMAKE_BUILD_TYPE STREQUAL "Debug")
				install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/opencore-amrwb.pdb
					DESTINATION bin
					PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
				)
			endif()
		endif()
	endif()
	target_include_directories(opencore-amrwb PRIVATE ${AMRWB_INCLUDE_DIRS})

	install(TARGETS opencore-amrwb
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)

	set(AMRWB_HEADER_FILES
		amrwb/dec_if.h
		amrwb/if_rom.h
	)

	install(FILES ${AMRWB_HEADER_FILES}
		DESTINATION include/opencore-amrwb
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	)
endif()

