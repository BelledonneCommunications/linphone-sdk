/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014  Belledonne Communications SARL http://www.belledonne-communications.com

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

#ifndef msformats_h
#define msformats_h

#ifdef __cplusplus
extern "C"{
#endif

/**
 * Simple enum to indicate whether a format is audio or video.
**/
typedef enum _MSFormatType{
	MSAudio,
	MSVideo,
	MSText,
	MSUnknownMedia
}MSFormatType;

/**
 * to string from enum.
**/
MS2_PUBLIC const char* ms_format_type_to_string(MSFormatType type);



/* those structs are part of the ABI: don't change their size otherwise binary plugins will be broken*/

typedef struct MSVideoSize{
	int width,height;
} MSVideoSize;


/**
 * Structure describing fully a media format.
**/
struct _MSFmtDescriptor{
	MSFormatType type; /**<format type, audio or video*/
	char *encoding; /**<the name of the encoding: for example pcmu, H264, opus*/
	int nchannels; /**<number of channels, relevant for audio only*/
	int rate; /**<Samplerate for audio, clockrate for video*/
	char *fmtp; /**<fmtp*/
	MSVideoSize vsize; /**<video size*/
	float fps; /**<average framerate*/
	char *text; /**<do not use directly, use ms_fmt_descriptor_to_string() instead*/
};

typedef struct _MSFmtDescriptor MSFmtDescriptor;

MS2_PUBLIC const char *ms_fmt_descriptor_to_string(const MSFmtDescriptor *orig);

MS2_PUBLIC bool_t ms_fmt_descriptor_equals(const MSFmtDescriptor *fmt1, const MSFmtDescriptor *fmt2);


#ifdef __cplusplus
}
#endif

#endif
