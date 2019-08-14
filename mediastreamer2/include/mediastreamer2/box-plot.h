/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MS2_BOX_PLOT_H
#define _MS2_BOX_PLOT_H

#include <stdint.h>

typedef struct _MSBoxPlot {
	int64_t min;
	int64_t max;
	double mean;
	double quad_moment; // E(X^2)
	uint64_t count;
} MSBoxPlot;

void ms_box_plot_reset(MSBoxPlot *bp);
void ms_box_plot_add_value(MSBoxPlot *bp, int64_t value);

double ms_box_plot_get_variance(const MSBoxPlot *bp);
double ms_box_plot_get_standard_deviation(const MSBoxPlot *bp);

char *ms_box_plot_to_string(const MSBoxPlot *bp, const char *unit);


typedef struct _MSUBoxPlot {
	uint64_t min;
	uint64_t max;
	double mean;
	double quad_moment; // E(X^2)
	uint64_t count;
} MSUBoxPlot;

void ms_u_box_plot_reset(MSUBoxPlot *bp);
void ms_u_box_plot_add_value(MSUBoxPlot *bp, uint64_t value);

double ms_u_box_plot_get_variance(const MSUBoxPlot *bp);
double ms_u_box_plot_get_standard_deviation(const MSUBoxPlot *bp);

char *ms_u_box_plot_to_string(const MSUBoxPlot *bp, const char *unit);


#endif // _MS2_BOX_PLOT_H
