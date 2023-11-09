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

#include <math.h>

#include <bctoolbox/port.h>

#include "mediastreamer2/box-plot.h"

#undef min
#undef max
#define min(x, y) (x < y) ? x : y;
#define max(x, y) (x < y) ? y : x;

void ms_box_plot_reset(MSBoxPlot *bp) {
	memset(bp, 0, sizeof(MSBoxPlot));
}

void ms_box_plot_add_value(MSBoxPlot *bp, int64_t value) {
	int64_t mean, deviation;
	if (bp->count == 0) {
		bp->min = bp->max = value;
	} else {
		bp->min = min(bp->min, value);
		bp->max = max(bp->max, value);
	}
	bp->sum += value;
	bp->count++;
	mean = bp->sum / bp->count;
	deviation = value - mean;
	bp->deviation_sum += deviation * deviation;
	bp->mean = (double)mean;
}

double ms_box_plot_get_variance(const MSBoxPlot *bp) {
	return bp->count != 0 ? (double)bp->deviation_sum / (double)bp->count : (double)0;
}

double ms_box_plot_get_standard_deviation(const MSBoxPlot *bp) {
	return sqrt(ms_box_plot_get_variance(bp));
}

char *ms_box_plot_to_string(const MSBoxPlot *bp, const char *unit) {
	if (unit == NULL) unit = "";
	return bctbx_strdup_printf("{ min=%lld%s, mean=%.1f%s, max=%lld%s }", (long long int)bp->min, unit, bp->mean, unit,
	                           (long long int)bp->max, unit);
}

void ms_u_box_plot_reset(MSUBoxPlot *bp) {
	memset(bp, 0, sizeof(MSUBoxPlot));
}

void ms_u_box_plot_add_value(MSUBoxPlot *bp, uint64_t value) {
	int64_t deviation, mean;
	if (bp->count == 0) {
		bp->min = bp->max = value;
	} else {
		bp->min = min(bp->min, value);
		bp->max = max(bp->max, value);
	}
	bp->sum += value;
	bp->count++;
	mean = bp->sum / bp->count;
	deviation = value - mean;
	bp->deviation_sum += deviation * deviation;
	bp->mean = (double)mean;
}

double ms_u_box_plot_get_variance(const MSUBoxPlot *bp) {
	return bp->count != 0 ? (double)bp->deviation_sum / (double)bp->count : (double)0;
}

double ms_u_box_plot_get_standard_deviation(const MSUBoxPlot *bp) {
	return sqrt(ms_u_box_plot_get_variance(bp));
}

char *ms_u_box_plot_to_string(const MSUBoxPlot *bp, const char *unit) {
	if (unit == NULL) unit = "";
	return bctbx_strdup_printf("{ min=%llu%s, mean=%.1f%s, max=%llu%s }", (long long unsigned)bp->min, unit, bp->mean,
	                           unit, (long long unsigned)bp->max, unit);
}
