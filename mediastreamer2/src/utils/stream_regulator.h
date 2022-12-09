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

#ifndef STREAM_REGULATOR_H
#define STREAM_REGULATOR_H

#include "ortp/str_utils.h"
#include "mediastreamer2/msticker.h"

/**
 * @brief MSStreamRegulator aims to synchronise a stream of mblk_t with a ticker
 */
typedef struct _MSStreamRegulator MSStreamRegulator;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an MSStreamRegulator
 * @param ticker Ticker which will be used to synchronise the mblkt
 * @param clock_rate Clock rate used to encode timestamp in the mblk_t
 * @return The pointer on the created MSStreamRegulator
 */
MSStreamRegulator *ms_stream_regulator_new(MSTicker *ticker, int64_t clock_rate);

/**
 * @brief Destroy an MSStreamRegulator
 * @param obj Stream regulator to destroy
 */
void ms_stream_regulator_free(MSStreamRegulator *obj);

/**
 * @brief Put an mblk_t buffer in the waiting queue of the stream regulator
 * @param obj MSStreamRegulator
 * @param pkt Buffer to store
 */
void ms_stream_regulator_push(MSStreamRegulator *obj, mblk_t *pkt);

/**
 * @brief Get the next waiting buffer.
 * If the timestamp of the next buffer is greater than ticker time, the buffer is unqueued
 * and the function return a pointer on it. Else, no buffer is unqueued and NULL is returned
 * @param obj MSStreamRegulator
 * @return Pointer on the unqueued buffer
 */
mblk_t *ms_stream_regulator_get(MSStreamRegulator *obj);

/**
 * @brief Reset the stream regulator
 * All waiting buffer are destroyed
 * @param obj MSStreamRegulator
 */
void ms_stream_regulator_reset(MSStreamRegulator *obj);

#ifdef __cplusplus
}
#endif

#endif
