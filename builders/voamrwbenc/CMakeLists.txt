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
project(VOAMRWBENC)


option(ENABLE_STATIC "Build static library (default is shared library)." NO)
option(ENABLE_ARMV5E "Enable ARMV5E assembler (default is no)." NO)
option(ENABLE_ARMV7NEON "Enable ARMV7 neon assembler (default is no)." NO)


set(SOURCE_FILES
	wrapper.c
	common/cmnMemory.c
	amrwbenc/src/autocorr.c
	amrwbenc/src/az_isp.c
	amrwbenc/src/bits.c
	amrwbenc/src/c2t64fx.c
	amrwbenc/src/c4t64fx.c
	amrwbenc/src/convolve.c
	amrwbenc/src/cor_h_x.c
	amrwbenc/src/decim54.c
	amrwbenc/src/deemph.c
	amrwbenc/src/dtx.c
	amrwbenc/src/g_pitch.c
	amrwbenc/src/gpclip.c
	amrwbenc/src/homing.c
	amrwbenc/src/hp400.c
	amrwbenc/src/hp50.c
	amrwbenc/src/hp6k.c
	amrwbenc/src/hp_wsp.c
	amrwbenc/src/int_lpc.c
	amrwbenc/src/isp_az.c
	amrwbenc/src/isp_isf.c
	amrwbenc/src/lag_wind.c
	amrwbenc/src/levinson.c
	amrwbenc/src/log2.c
	amrwbenc/src/lp_dec2.c
	amrwbenc/src/math_op.c
	amrwbenc/src/mem_align.c
	amrwbenc/src/oper_32b.c
	amrwbenc/src/p_med_ol.c
	amrwbenc/src/pit_shrp.c
	amrwbenc/src/pitch_f4.c
	amrwbenc/src/pred_lt4.c
	amrwbenc/src/preemph.c
	amrwbenc/src/q_gain2.c
	amrwbenc/src/q_pulse.c
	amrwbenc/src/qisf_ns.c
	amrwbenc/src/qpisf_2s.c
	amrwbenc/src/random.c
	amrwbenc/src/residu.c
	amrwbenc/src/scale.c
	amrwbenc/src/stream.c
	amrwbenc/src/syn_filt.c
	amrwbenc/src/updt_tar.c
	amrwbenc/src/util.c
	amrwbenc/src/voAMRWBEnc.c
	amrwbenc/src/voicefac.c
	amrwbenc/src/wb_vad.c
	amrwbenc/src/weight_a.c
)
if(ENABLE_ARMV5E)
	list(APPEND SOURCE_FILES
		amrwbenc/src/asm/ARMV5E/convolve_opt.s
		amrwbenc/src/asm/ARMV5E/cor_h_vec_opt.s
		amrwbenc/src/asm/ARMV5E/Deemph_32_opt.s
		amrwbenc/src/asm/ARMV5E/Dot_p_opt.s
		amrwbenc/src/asm/ARMV5E/Filt_6k_7k_opt.s
		amrwbenc/src/asm/ARMV5E/Norm_Corr_opt.s
		amrwbenc/src/asm/ARMV5E/pred_lt4_1_opt.s
		amrwbenc/src/asm/ARMV5E/residu_asm_opt.s
		amrwbenc/src/asm/ARMV5E/scale_sig_opt.s
		amrwbenc/src/asm/ARMV5E/Syn_filt_32_opt.s
		amrwbenc/src/asm/ARMV5E/syn_filt_opt.s
	)
endif()
if(ENABLE_ARMV7NEON)
	list(APPEND SOURCE_FILES
		amrwbenc/src/asm/ARMV7/convolve_neon.s
		amrwbenc/src/asm/ARMV7/cor_h_vec_neon.s
		amrwbenc/src/asm/ARMV7/Deemph_32_neon.s
		amrwbenc/src/asm/ARMV7/Dot_p_neon.s
		amrwbenc/src/asm/ARMV7/Filt_6k_7k_neon.s
		amrwbenc/src/asm/ARMV7/Norm_Corr_neon.s
		amrwbenc/src/asm/ARMV7/pred_lt4_1_neon.s
		amrwbenc/src/asm/ARMV7/residu_asm_neon.s
		amrwbenc/src/asm/ARMV7/scale_sig_neon.s
		amrwbenc/src/asm/ARMV7/Syn_filt_32_neon.s
		amrwbenc/src/asm/ARMV7/syn_filt_neon.s
	)
endif()
if(WIN32)
	file(READ ${CMAKE_CURRENT_SOURCE_DIR}/vo-amrwbenc.sym VOAMRWBENC_SYM)
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/vo-amrwbenc.def "LIBRARY vo-amrwbenc\nEXPORTS\n${VOAMRWBENC_SYM}")
	list(APPEND SOURCE_FILES ${CMAKE_CURRENT_BINARY_DIR}/vo-amrwbenc.def)
endif()

if(ENABLE_ARMV5E OR ENABLE_ARMV7NEON)
	if(ANDROID AND CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
		set(ARCH_OPTIONS "-mfpu=neon")
	endif()
	get_directory_property(DIRECTORY_PROPERTY_COMPILE_OPTIONS "COMPILE_OPTIONS")
	string(REPLACE ";" " " DIRECTORY_PROPERTY_COMPILE_OPTIONS "${DIRECTORY_PROPERTY_COMPILE_OPTIONS}")
	set(CMAKE_ASM_TARGET_FLAG "")
	if(CMAKE_C_COMPILER_TARGET)
		set(CMAKE_ASM_TARGET_FLAG "--target=${CMAKE_C_COMPILER_TARGET}")
	endif()
	set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> ${CMAKE_ASM_TARGET_FLAG} ${CMAKE_CPP_FLAGS} ${CMAKE_C_FLAGS} ${DIRECTORY_PROPERTY_COMPILE_OPTIONS} ${ARCH_OPTIONS} -o <OBJECT> -c <SOURCE>")
	enable_language(ASM)
endif()

if(ENABLE_STATIC)
	add_library(vo-amrwbenc STATIC ${SOURCE_FILES})
else()
	add_library(vo-amrwbenc SHARED ${SOURCE_FILES})
	set_target_properties(vo-amrwbenc PROPERTIES VERSION 0.0.4)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/vo-amrwbenc.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()

target_compile_options(vo-amrwbenc PRIVATE "-w")
if(MSVC)
	target_compile_options(vo-amrwbenc PRIVATE "/Ob0")	# Deactivate inlining to prevent linking errors
endif()

target_include_directories(vo-amrwbenc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/amrwbenc/inc ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
if(ENABLE_ARMV5E)
	target_compile_definitions(vo-amrwbenc PRIVATE "-DARM" "-DASM_OPT")
endif()
if(ENABLE_ARMV7NEON)
	target_compile_definitions(vo-amrwbenc PRIVATE "-DARM" "-DARMV7" "-DASM_OPT")
endif()

install(TARGETS vo-amrwbenc
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(FILES enc_if.h
	DESTINATION include/vo-amrwbenc
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
