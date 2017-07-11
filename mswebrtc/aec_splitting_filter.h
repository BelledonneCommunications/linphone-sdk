/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2017  Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef AEC_SPLITTING_FILTER_H
#define AEC_SPLITTING_FILTER_H


#ifdef __cplusplus
extern "C" {
#endif


typedef struct MSWebRtcAecSplittingFilterStruct MSWebRtcAecSplittingFilter;


MSWebRtcAecSplittingFilter * mswebrtc_aec_splitting_filter_create(int nbands, int bandsize);

void mswebrtc_aec_splitting_filter_destroy(MSWebRtcAecSplittingFilter *filter);

void mswebrtc_aec_splitting_filter_analysis(MSWebRtcAecSplittingFilter *filter, int16_t *ref, int16_t *echo);

void mswebrtc_aec_splitting_filter_synthesis(MSWebRtcAecSplittingFilter *filter, int16_t *oecho);

float * mswebrtc_aec_splitting_filter_get_ref(MSWebRtcAecSplittingFilter *filter);

const float * const * mswebrtc_aec_splitting_filter_get_echo_bands(MSWebRtcAecSplittingFilter *filter);

float * const * mswebrtc_aec_splitting_filter_get_output_bands(MSWebRtcAecSplittingFilter *filter);

int mswebrtc_aec_splitting_filter_get_number_of_bands(MSWebRtcAecSplittingFilter *filter);

int mswebrtc_aec_splitting_filter_get_bandsize(MSWebRtcAecSplittingFilter *filter);


#ifdef __cplusplus
}
#endif


#endif /* AEC_SPLITTING_FILTER_H */