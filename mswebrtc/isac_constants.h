/*
 constants.h
 Copyright (C) 2013 Belledonne Communications, Grenoble, France

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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef ISAC_CONSTANTS_H
#define ISAC_CONSTANTS_H

/* Define codec specific settings */

#define ISAC_SAMPLE_RATE 16000

/* (10 / 1000) * SAMPLE_RATE, the number of samples to pass at each
 * WebRtcIsacFix_Encode() call */
#define ISAC_SAMPLES_PER_ENCODE 160

typedef enum {
	ISAC_60MS_SAMPLE_COUNT = 960,
	ISAC_30MS_SAMPLE_COUNT = 480
} isac_sample_count_e;

/* This enum is to be used in WebRtcIsacfix_EncoderInit() */
typedef enum {
	/* In this mode, the bitrate and ptime are adjusted according to statistics
	 * provided by the user through the WebRtcIsacfix_UpdateBwEstimate*()
	 * functions */
	CODING_AUTOMATIC,
	/* Setup the encoder so that bitrate and ptime are controlled by calls to
	 * WebRtcIsacFix_Encode(). This means the user is in charge of evaluating
	 * the correct set of parameters for optimal call quality. */
	CODING_USERDEFINED
} isac_codingmode_e;

#define ISAC_BITRATE_MAX 32000
#define ISAC_BITRATE_MIN 10000

#endif // ISAC_CONSTANTS_H
