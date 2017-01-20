############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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

find_library(LIBM NAMES m)
if(ANDROID)
	find_library(ANDROID_CPU_FEATURES_LIBRARY NAMES cpufeatures)
	find_library(ANDROID_SUPPORT_LIBRARY NAMES support)
endif()

if(MSVC AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	find_library(LIBGCC NAMES gcc)
	find_library(LIBMINGWEX NAMES mingwex)
endif()

find_program(AWK_PROGRAM awk)
if(NOT AWK_PROGRAM)
	message(FATAL_ERROR "You need the awk program to build mediastreamer2.")
endif()

set(BASE_SOURCE_FILES_C
	base/eventqueue.c
	base/mscommon.c
	base/msfactory.c
	base/msfilter.c
	base/msqueue.c
	base/mssndcard.c
	base/msticker.c
	base/msvideopresets.c
	base/mswebcam.c
	base/mtu.c
	base/msasync.c
	otherfilters/itc.c
	otherfilters/join.c
	otherfilters/tee.c
	otherfilters/void.c
)
if(ANDROID)
	list(APPEND BASE_SOURCE_FILES_C utils/msjava.c)
endif()

find_package(Git)
add_custom_target(ms2-git-version
	COMMAND ${CMAKE_COMMAND} -DGIT_EXECUTABLE=${GIT_EXECUTABLE} -DMEDIASTREAMER_VERSION=${MEDIASTREAMER_VERSION} -DWORK_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/gitversion.cmake
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/gitversion.h"
)
add_custom_target(ms2-basedescs-header
	COMMAND ${CMAKE_COMMAND} -DAWK_PROGRAM=${AWK_PROGRAM} -DAWK_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../"
		-DINPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
		-DTYPE=base -DSOURCE_FILES="${BASE_SOURCE_FILES_C}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/generate_descs_header.cmake"
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/basedescs.h")

add_definitions(
	-DMEDIASTREAMER2_EXPORTS
	-DMEDIASTREAMER2_INTERNAL_EXPORTS
	-D_TRUE_TIME
	-DMS2_INTERNAL
	-DMS2_FILTERS
)

set(BASE_LIBS
	${BCTOOLBOX_CORE_LIBRARIES} ${ORTP_LIBRARIES}
)
if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	list(APPEND BASE_LIBS ${LIBGCC} ${LIBMINGWEX} ole32 oleaut32 uuid Winmm Ws2_32)
endif()
if(HAVE_DLOPEN)
	list(APPEND BASE_LIBS dl)
endif()

apply_compile_flags(BASE_SOURCE_FILES_C "CPP" "C")

if(ENABLE_STATIC)
	add_library(mediastreamer_base-static STATIC ${MEDIASTREAMER2_HEADER_FILES} ${BASE_SOURCE_FILES_C})
	set_target_properties(mediastreamer_base-static PROPERTIES OUTPUT_NAME mediastreamer_base)
	target_link_libraries(mediastreamer_base-static ${BASE_LIBS})
	add_dependencies(mediastreamer_base-static ms2-git-version ms2-basedescs-header)
	install(TARGETS mediastreamer_base-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(mediastreamer_base SHARED ${MEDIASTREAMER2_HEADER_FILES} ${BASE_SOURCE_FILES_C})
	set_target_properties(mediastreamer_base PROPERTIES LINKER_LANGUAGE C)
	if(ANDROID)
		set_target_properties(mediastreamer_base PROPERTIES OUTPUT_NAME "mediastreamer_base-${NDK_ARCHITECTURE}")
	else()
		# Do not version shared library on Android
		set_target_properties(mediastreamer_base PROPERTIES SOVERSION ${MEDIASTREAMER_SO_VERSION})
	endif()
	target_link_libraries(mediastreamer_base PRIVATE ${BASE_LIBS})
	add_dependencies(mediastreamer_base ms2-git-version ms2-basedescs-header)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/mediastreamer_base.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	install(TARGETS mediastreamer_base EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()



set(VOIP_SOURCE_FILES_C
	audiofilters/alaw.c
	audiofilters/audiomixer.c
	audiofilters/chanadapt.c
	audiofilters/devices.c
	audiofilters/dtmfgen.c
	audiofilters/equalizer.c
	audiofilters/flowcontrol.c
	audiofilters/g711.c
	audiofilters/g711.h
	audiofilters/genericplc.h
	audiofilters/genericplc.c
	audiofilters/msgenericplc.c
	audiofilters/l16.c
	audiofilters/msfileplayer.c
	audiofilters/msfilerec.c
	audiofilters/asyncrw.c
	audiofilters/asyncrw.h
	audiofilters/msg722.c
	audiofilters/msvaddtx.c
	audiofilters/msvolume.c
	audiofilters/tonedetector.c
	audiofilters/ulaw.c
	audiofilters/waveheader.h
	crypto/dtls_srtp.c
	crypto/ms_srtp.c
	crypto/zrtp.c
	otherfilters/msrtp.c
	utils/_kiss_fft_guts.h
	utils/audiodiff.c
	utils/dsptools.c
	utils/g722.h
	utils/g722_decode.c
	utils/g722_encode.c
	utils/h264utils.h
	utils/h264utils.c
	utils/kiss_fft.c
	utils/kiss_fft.h
	utils/kiss_fftr.c
	utils/kiss_fftr.h
	utils/pcap_sender.c
	utils/pcap_sender.h
	utils/stream_regulator.c
	voip/audioconference.c
	voip/audiostream.c
	voip/bitratecontrol.c
	voip/bitratedriver.c
	voip/ice.c
	voip/mediastream.c
	voip/msiframerequestslimiter.c
	voip/msmediaplayer.c
	voip/msvoip.c
	voip/private.h
	voip/qosanalyzer.c
	voip/qosanalyzer.h
	voip/qualityindicator.c
	otherfilters/rfc4103_source.c
	otherfilters/rfc4103_sink.c
	otherfilters/msudp.c
	voip/rfc4103_textstream.c
	voip/ringstream.c
	voip/stun.c
)
set(VOIP_SOURCE_FILES_CXX )
set(VOIP_SOURCE_FILES_OBJC )
set(VOIP_SOURCE_FILES_ASM )

if(ENABLE_ALSA)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/alsa.c)
endif()
if(ANDROID)
	list(APPEND VOIP_SOURCE_FILES_CXX voip/msvoip_jni.cc)
endif()
if(ENABLE_ANDROIDSND)
	list(APPEND VOIP_SOURCE_FILES_CXX
		android/androidsound.cpp
		android/androidsound_depr.cpp
		android/androidsound_opensles.cpp
		android/AudioRecord.cpp
		android/AudioSystem.cpp
		android/AudioTrack.cpp
		android/hardware_echo_canceller.cpp
		android/loader.cpp
		android/String8.cpp
		#audiofilters/aac-eld-android.cpp
		#Not working on Android, to be fix
	)
endif()
if(ENABLE_ARTSC)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/arts.c)
endif()
if(ENABLE_MACSND)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/macsnd.c)
endif()
if(ENABLE_MACAQSND)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/aqsnd.c)
endif()
if(ENABLE_OSS)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/oss.c)
endif()
if(ENABLE_PORTAUDIO)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/pasnd.c)
endif()
if(ENABLE_PULSEAUDIO)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/pulseaudio.c)
endif()
if(ENABLE_QSA)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/qsa.c)
endif()
if(APPLE)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/aac-eld.c)
endif()
if(IOS)
	list(APPEND VOIP_SOURCE_FILES_OBJC audiofilters/msiounit.m)
endif()
if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/winsnd3.c)
endif()

if(ENABLE_G726)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/g726.c)
endif()
if(GSM_FOUND)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/gsm.c)
endif()
if(BV16_FOUND)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/bv16.c)
endif()
if(OPUS_FOUND)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/msopus.c)
endif()
if(ENABLE_RESAMPLE)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/msresample.c)
endif()
if(SPEEX_FOUND)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/msspeex.c)
endif()
if(SPEEXDSP_FOUND)
	list(APPEND VOIP_SOURCE_FILES_C audiofilters/speexec.c)
endif()

if(ENABLE_VIDEO)
	list(APPEND VOIP_SOURCE_FILES_C
		utils/bits_rw.c
		videofilters/extdisplay.c
		videofilters/mire.c
		videofilters/nowebcam.c
		videofilters/pixconv.c
		videofilters/sizeconv.c
		voip/layouts.c
		voip/layouts.h
		voip/msvideo.c
		voip/msvideo_neon.c
		voip/msvideo_neon.h
		voip/nowebcam.h
		voip/rfc2429.h
		voip/rfc3984.c
		voip/videostarter.c
		voip/videostream.c
		voip/video_preset_high_fps.c
	)
	if(FFMPEG_FOUND AND NOT TURBOJPEG_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C
			videofilters/ffmpegnowebcam.c
		)
	elseif(TURBOJPEG_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C
			videofilters/turbojpegnowebcam.c
		)
	endif()
	if(WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		add_definitions(-DHAVE_DIRECTSHOW)
		list(APPEND VOIP_SOURCE_FILES_C
			videofilters/drawdib-display.c
		)
		list(APPEND VOIP_SOURCE_FILES_CXX
			videofilters/msdscap.cc
		)
	endif()
	if(APPLE)
		if(IOS)
			list(APPEND VOIP_SOURCE_FILES_C
				utils/opengles_display.c
				utils/opengles_display.h
				utils/shaderUtil.c
				utils/shaderUtil.h
				voip/scaler.c
			)
			list(APPEND VOIP_SOURCE_FILES_OBJC
				videofilters/iosdisplay.m
				videofilters/ioscapture.m
				voip/ioshardware.m
			)
		else()
			list(APPEND VOIP_SOURCE_FILES_OBJC
				videofilters/msosxdisplay.m
				videofilters/avfound-capture.m
			)

			list(APPEND VOIP_SOURCE_FILES_C
				utils/opengles_display.c
				utils/opengles_display.h
				utils/shaderUtil.c
				utils/shaderUtil.h
			)
		endif()
	endif()
	if(APPLE)
		list(APPEND VOIP_SOURCE_FILES_C videofilters/videotoolbox.c)
	endif()
	if(ANDROID)
		list(APPEND VOIP_SOURCE_FILES_C
			android/mediacodech264dec.c
			android/mediacodech264enc.c
			android/android-display.c
			android/android-opengl-display.c
			utils/opengles_display.c
			utils/opengles_display.h
			utils/shaderUtil.c
			utils/shaderUtil.h
			voip/scaler.c
		)
		if(CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
			list(APPEND VOIP_SOURCE_FILES_ASM
				voip/scaler_arm.S
			)
		endif()
		list(APPEND VOIP_SOURCE_FILES_CXX
			android/android_mediacodec.cpp
			android/android-display-bad.cpp
			android/androidvideo.cpp
		)
	endif()
	if (ENABLE_QNX)
		list(APPEND VOIP_SOURCE_FILES_CXX
			videofilters/bb10_display.cpp
			videofilters/bb10_capture.cpp
		)
	endif()
	if(FFMPEG_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C
			utils/ffmpeg-priv.c
			utils/ffmpeg-priv.h
			utils/swscale.h
			videofilters/h264dec.c
			videofilters/videodec.c
			videofilters/videoenc.c
		)
		if(NOT TURBOJPEG_FOUND)
			list(APPEND VOIP_SOURCE_FILES_C
				videofilters/ffmpegjpegwriter.c
			)
		endif()
	endif()
	if(TURBOJPEG_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C
			videofilters/turbojpegwriter.c
		)
	endif()
	if(GLX_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C
			utils/opengles_display.c
			utils/shaderUtil.c
			videofilters/glxvideo.c
		)
	endif()
	if(SDL_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C videofilters/videoout.c)
	endif()
	if(THEORA_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C videofilters/theora.c)
	endif()
	if(V4L_FOUND)
		if(HAVE_LINUX_VIDEODEV_H)
			list(APPEND VOIP_SOURCE_FILES_C videofilters/msv4l.c)
		endif()
		if(HAVE_LINUX_VIDEODEV2_H)
			list(APPEND VOIP_SOURCE_FILES_C videofilters/msv4l2.c)
		endif()
	endif()
	if(VPX_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C
			videofilters/vp8.c
			voip/vp8rtpfmt.c
			voip/vp8rtpfmt.h
		)
	endif()
	if(X11_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C utils/x11_helper.c)
	endif()
	if(XV_FOUND)
		list(APPEND VOIP_SOURCE_FILES_C videofilters/x11video.c)
	endif()
endif()
if(MATROSKA2_FOUND)
	list(APPEND VOIP_SOURCE_FILES_C
		videofilters/mkv.c
		utils/mkv_reader.c
		voip/rfc2429.h
		voip/rfc3984.c
	)
endif()

if(PCAP_FOUND)
	list(APPEND VOIP_SOURCE_FILES_C src/otherfilters/mspcapfileplayer.c)
endif()

set(VOIP_SOURCE_FILES_ALL ${VOIP_SOURCE_FILES_C} ${VOIP_SOURCE_FILES_CXX} ${VOIP_SOURCE_FILES_OBJC})

add_custom_target(ms2-voipdescs-header
	COMMAND ${CMAKE_COMMAND} -DAWK_PROGRAM=${AWK_PROGRAM} -DAWK_SCRIPTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../"
		-DINPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
		-DTYPE=voip -DSOURCE_FILES="${VOIP_SOURCE_FILES_ALL}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/generate_descs_header.cmake"
	BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/voipdescs.h"
)

if(APPLE OR ANDROID OR GLX_FOUND)
	find_package(PythonInterp REQUIRED)
	find_program(SED_PROGRAM sed)
	if(NOT SED_PROGRAM)
		message(FATAL_ERROR "You need the sed program to build mediastreamer2.")
	endif()
	add_custom_target(ms2-yuv2rgb-fs-header
		COMMAND ${CMAKE_COMMAND} -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} -DSED_PROGRAM=${SED_PROGRAM}
			-DINPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
			-DSOURCE_FILE="yuv2rgb.fs"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/generate_yuv2rgb_header.cmake"
		BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/yuv2rgb.fs.h"
	)
	add_custom_target(ms2-yuv2rgb-vs-header
		COMMAND ${CMAKE_COMMAND} -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} -DSED_PROGRAM=${SED_PROGRAM}
			-DINPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
			-DSOURCE_FILE="yuv2rgb.vs"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/generate_yuv2rgb_header.cmake"
		BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/yuv2rgb.vs.h"
	)
endif()

apply_compile_flags(VOIP_SOURCE_FILES_C "CPP" "C")
apply_compile_flags(VOIP_SOURCE_FILES_OBJC "CPP" "OBJC")
if(VOIP_SOURCE_FILES_CXX)
	set_source_files_properties(${VOIP_SOURCE_FILES_CXX} PROPERTIES LANGUAGE CXX)
	apply_compile_flags(VOIP_SOURCE_FILES_CXX "CPP" "CXX")
endif()


if(ENABLE_VIDEO AND ANDROID AND CMAKE_SYSTEM_PROCESSOR STREQUAL "armeabi-v7a")
	set(NEON_SOURCE_FILES
		voip/msvideo_neon.c
		voip/scaler.c
	)
	foreach(file ${NEON_SOURCE_FILES})
		get_source_file_property(NEON_COMPILE_FLAGS ${file} COMPILE_FLAGS)
		set_source_files_properties(${file} PROPERTIES COMPILE_FLAGS "${NEON_COMPILE_FLAGS} -mfpu=neon")
	endforeach()
endif()


if(VOIP_SOURCE_FILES_ASM)
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


set(VOIP_LIBS
	${BCTOOLBOX_CORE_LIBRARIES}
	${ORTP_LIBRARIES}
)
if(WIN32)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND NOT CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
		list(APPEND VOIP_LIBS ${LIBGCC} ${LIBMINGWEX} ole32 oleaut32 uuid Ws2_32)
		if(ENABLE_VIDEO)
			list(APPEND VOIP_LIBS gdi32 strmiids user32 vfw32)
		endif()
	endif()
endif()
if(LIBM)
	list(APPEND VOIP_LIBS ${LIBM})
endif()
if(ANDROID_CPU_FEATURES_LIBRARY)
	list(APPEND VOIP_LIBS ${ANDROID_CPU_FEATURES_LIBRARY})
endif()
if(ANDROID_SUPPORT_LIBRARY)
	list(APPEND VOIP_LIBS ${ANDROID_SUPPORT_LIBRARY})
endif()
if(ANDROID AND ENABLE_VIDEO)
	list(APPEND VOIP_LIBS GLESv2)
endif()
if(HAVE_DLOPEN)
	list(APPEND VOIP_LIBS dl)
endif()
if(PCAP_FOUND)
	list(APPEND VOIP_LIBS ${PCAP_LIBRARIES})
endif()
if(BZRTP_FOUND)
	list(APPEND VOIP_LIBS ${BZRTP_LIBRARIES})
endif()
if(SRTP_FOUND)
	list(APPEND VOIP_LIBS ${SRTP_LIBRARIES})
endif()
if(SPANDSP_FOUND)
	list(APPEND VOIP_LIBS ${SPANDSP_LIBRARIES})
endif()
if(GSM_FOUND)
	list(APPEND VOIP_LIBS ${GSM_LIBRARIES})
endif()
if(OPUS_FOUND)
	list(APPEND VOIP_LIBS ${OPUS_LIBRARIES})
endif()
if(SPEEX_FOUND)
	list(APPEND VOIP_LIBS ${SPEEX_LIBRARIES})
endif()
if(SPEEXDSP_FOUND)
	list(APPEND VOIP_LIBS ${SPEEXDSP_LIBRARIES})
endif()
if(BV16_FOUND)
	list(APPEND VOIP_LIBS ${BV16_LIBRARIES})
endif()
if(ALSA_FOUND)
	list(APPEND VOIP_LIBS ${ALSA_LIBRARIES})
endif()
if(ARTS_FOUND)
	list(APPEND VOIP_LIBS ${ARTS_LIBRARIES})
endif()
if(PORTAUDIO_FOUND)
	list(APPEND VOIP_LIBS ${PORTAUDIO_LIBRARIES})
endif()
if(PULSEAUDIO_FOUND)
	list(APPEND VOIP_LIBS ${PULSEAUDIO_LIBRARIES})
endif()
if(QSA_FOUND)
	list(APPEND VOIP_LIBS ${QSA_LIBRARIES})
	list(APPEND VOIP_LIBS ${QNXAUDIOMANAGER_LIBRARIES})
endif()
if(FFMPEG_FOUND)
	list(APPEND VOIP_LIBS ${FFMPEG_LIBRARIES})
	if(MSVC)
		add_definitions(-Dinline=__inline)
	endif()
endif()
if(V4L_FOUND)
	list(APPEND VOIP_LIBS ${V4L_LIBRARIES})
endif()
if(X11_FOUND)
	list(APPEND VOIP_LIBS ${X11_LIBRARIES})
endif()
if(XV_FOUND)
	list(APPEND VOIP_LIBS ${XV_LIBRARIES})
endif()
if(GLX_FOUND)
	list(APPEND VOIP_LIBS ${GLX_LIBRARIES})
endif()
if(THEORA_FOUND)
	list(APPEND VOIP_LIBS ${THEORA_LIBRARIES})
endif()
if(VPX_FOUND)
	list(APPEND VOIP_LIBS ${VPX_LIBRARIES})
endif()
if(MATROSKA2_FOUND)
	list(APPEND VOIP_LIBS ${MATROSKA2_LIBRARIES})
endif()
if (SCREEN_FOUND)
	list(APPEND VOIP_LIBS ${SCREEN_LIBRARIES})
endif()
if (CAMAPI_FOUND)
	list(APPEND VOIP_LIBS ${CAMAPI_LIBRARIES})
endif()
if(TURBOJPEG_FOUND)
	list(APPEND VOIP_LIBS ${TURBOJPEG_LIBRARIES})
endif()
if (BCGG729_FOUND)
	 add_definitions(-DHAVE_G729B=1)
	list(APPEND VOIP_LIBS ${BCG729_LIBRARIES})
endif()
set(MS2_VOIP_DEPENDENCIES ms2-git-version ms2-voipdescs-header)
if(APPLE OR ANDROID OR GLX_FOUND)
	list(APPEND MS2_VOIP_DEPENDENCIES ms2-yuv2rgb-fs-header ms2-yuv2rgb-vs-header)
endif()

string(REPLACE ";" " " LINK_FLAGS_STR "${LINK_FLAGS}")
if(ENABLE_STATIC)
	add_library(mediastreamer_voip-static STATIC ${MEDIASTREAMER2_HEADER_FILES} ${VOIP_SOURCE_FILES_C} ${VOIP_SOURCE_FILES_CXX} ${VOIP_SOURCE_FILES_OBJC} ${VOIP_SOURCE_FILES_ASM})
	set_target_properties(mediastreamer_voip-static PROPERTIES OUTPUT_NAME mediastreamer_voip)
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer_voip-static PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	add_dependencies(mediastreamer_voip-static ${MS2_VOIP_DEPENDENCIES})
	target_link_libraries(mediastreamer_voip-static mediastreamer_base-static ${VOIP_LIBS})
	install(TARGETS mediastreamer_voip-static EXPORT ${EXPORT_TARGETS_NAME}Targets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()
if(ENABLE_SHARED)
	add_library(mediastreamer_voip SHARED ${MEDIASTREAMER2_HEADER_FILES} ${VOIP_SOURCE_FILES_C} ${VOIP_SOURCE_FILES_CXX} ${VOIP_SOURCE_FILES_OBJC} ${VOIP_SOURCE_FILES_ASM})
	set_target_properties(mediastreamer_voip PROPERTIES LINKER_LANGUAGE CXX)
	if(LINK_FLAGS_STR)
		set_target_properties(mediastreamer_voip PROPERTIES LINK_FLAGS "${LINK_FLAGS_STR}")
	endif()
	add_dependencies(mediastreamer_voip ${MS2_VOIP_DEPENDENCIES})
	if(ANDROID)
		set_target_properties(mediastreamer_voip PROPERTIES OUTPUT_NAME "mediastreamer_voip-${NDK_ARCHITECTURE}")
	else()
		# Do not version shared library on Android
		set_target_properties(mediastreamer_voip PROPERTIES SOVERSION ${MEDIASTREAMER_SO_VERSION})
	endif()
	target_link_libraries(mediastreamer_voip mediastreamer_base ${VOIP_LIBS})
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}/mediastreamer_voip.pdb
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
	install(TARGETS mediastreamer_voip EXPORT ${EXPORT_TARGETS_NAME}Targets
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
	)
endif()

if(FFMPEG_FOUND AND NOT WIN32)
	set(NO_DEPRECATED_DECLARATIONS_SOURCE_FILES
		videofilters/videoenc.c
		videofilters/videodec.c
		videofilters/h264dec.c
	)
	if(NOT TURBOJPEG_FOUND)
		set(NO_DEPRECATED_DECLARATIONS_SOURCE_FILES
			${NO_DEPRECATED_DECLARATIONS_SOURCE_FILES}
			videofilters/ffmpegjpegwriter.c
			videofilters/nowebcam.c
		)
	endif()
	foreach(file ${NO_DEPRECATED_DECLARATIONS_SOURCE_FILES})
		get_source_file_property(NO_DEPRECATED_DECLARATIONS_COMPILE_FLAGS ${file} COMPILE_FLAGS)
		set_source_files_properties(${file} PROPERTIES COMPILE_FLAGS "${NO_DEPRECATED_DECLARATIONS_COMPILE_FLAGS} -Wno-error=deprecated-declarations")
	endforeach()
endif()

if(ENABLE_VIDEO)
	install(FILES voip/nowebcamCIF.jpg
		DESTINATION ${PACKAGE_DATA_DIR}/images
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
	)
endif()
