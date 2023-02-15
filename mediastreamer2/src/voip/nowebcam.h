/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef nowebcam_h
#define nowebcam_h

#include "mediastreamer2/formats.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfactory.h"

#ifdef __cplusplus
extern "C" {
#endif
MS2_PUBLIC mblk_t *ms_load_jpeg_as_yuv(const char *path, MSVideoSize *reqsize);
MS2_PUBLIC mblk_t *ms_load_nowebcam(MSFactory *factory, MSVideoSize *reqsize, int idx);
MS2_PUBLIC mblk_t *jpeg2yuv(uint8_t *jpgbuf, int bufsize, MSVideoSize *reqsize);

#ifdef __cplusplus
}
#endif

#endif
