/*
filter-template.c

mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011-2012 Belledonne Communications, Grenoble, France

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


#include "mediastreamer2/msfilter.h"


/******************************************************************************
 * This file is mediastreamer2 filter template to be used to create new       *
 * filters.                                                                   *
 * The procedure to follow to create a new filter is to:                      *
 *  1. Copy this file to the file that will be your new filter                *
 *  2. Replace any "MyFilter" and "my_filter" occurences to the name that     *
 *     suits your new filter                                                  *
 *  3. Fill in the blanks where are located comments beginning with           *
 *     FILTER_TODO                                                            *
 *****************************************************************************/


/**
 * Definition of the private data structure of the filter.
 */
typedef struct _MyFilterData {
	char some_value;
	/* FILTER_TODO: Fill the private data structure with the data fields
	   that are required to run your filter. */
} MyFilterData;


/******************************************************************************
 * Methods to (de)initialize and run the filter                               *
 *****************************************************************************/

/**
 * Initialization of the filter.
 *
 * This function is called once when the filter is created by calling
 * ms_filter_new(), ms_filter_new_from_desc(), ms_filter_new_from_name(),
 * ms_filter_create_encoder() or ms_filter_create_decoder().
 */
static void my_filter_init(MSFilter *f) {
	MyFilterData *d = (MyFilterData *)ms_new(MyFilterData, 1);
	d->some_value = 42;
	/* FILTER_TODO: Initialize the filter private data and eventually
	   call initialization functions of the libraries on which the filter
	   depends. */
	f->data = d;
}

/**
 * Preprocessing performed when the filter is attached to a ticker.
 *
 * This function is called only once.
 */
static void my_filter_preprocess(MSFilter *f) {
	/* FILTER_TODO: Fill with the implementation specific to the filter. */
}

/**
 * Core of the filter: this is where the processing is done.
 *
 * The principle is to read data from the input(s), perform some processing on
 * it, and then write it to the output(s).
 *
 * This function is called at each tick of the ticker to which the filter has
 * been attached to.
 */
static void my_filter_process(MSFilter *f) {
	/* FILTER_TODO: Fill with the implementation specific to the filter. */
}

/**
 * Postprocessing performed when the filter is detached from the ticker it was
 * attached to.
 *
 * This function is called only once.
 */
static void my_filter_postprocess(MSFilter *f) {
	/* FILTER_TODO: Fill with the implementation specific to the filter. */
}

/**
 * Uninitialization of the filter.
 *
 * This function is called once when the filter is destroyed by calling
 * ms_filter_destroy().
 */
static void my_filter_uninit(MSFilter *f) {
	MyFilterData *d = (MyFilterData *)f->data;
	/* FILTER_TODO: Uninitialize the filter private data and eventually
	   call the uninitialization functions of the libraries on which the
	   filter depends. */
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the filter                                            *
 *****************************************************************************/

static int my_filter_set_sample_rate(MSFilter *f, void *arg) {
	/* FILTER_TODO: Fill with real implementation. */
	return 0;
}

static int my_filter_get_sample_rate(MSFilter *f, void *arg) {
	/* FILTER_TODO: Fill with real implementation. */
	return 0;
}

/**
 * This array defines the list of functions that can be called to configure
 * the filter.
 *
 * The mean to call these functions is to call the ms_filter_call_method() or
 * ms_filter_call_method_noarg() functions.
 *
 * The array must be NULL terminated.
 */
static MSFilterMethod my_filter_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	my_filter_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	my_filter_get_sample_rate	},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the filter                                                   *
 *****************************************************************************/

/* MY_FILTER_ID is to be added in from allfilters.h */
#define MY_FILTER_NAME		"MyFilter"
#define MY_FILTER_DESCRIPTION	"An empty filter."
#define MY_FILTER_CATEGORY	MS_FILTER_OTHER
#define MY_FILTER_ENC_FMT	NULL
#define MY_FILTER_NINPUTS	1
#define MY_FILTER_NOUTPUTS	0
#define MY_FILTER_FLAGS		0

#ifndef _MSC_VER

MSFilterDesc my_filter_desc = {
	.id = MY_FILTER_ID,
	.name = MY_FILTER_NAME,
	.text = MY_FILTER_DESCRIPTION,
	.category = MY_FILTER_CATEGORY,
	.enc_fmt = MY_FILTER_ENC_FMT,
	.ninputs = MY_FILTER_NINPUTS,
	.noutputs = MY_FILTER_NOUTPUTS,
	.init = my_filter_init,
	.preprocess = my_filter_preprocess,
	.process = my_filter_process,
	.postprocess = my_filter_postprocess,
	.uninit = my_filter_uninit,
	.methods = my_filter_methods,
	.flags = MY_FILTER_FLAGS
};

#else

MSFilterDesc my_filter_desc = {
	MY_FILTER_ID,
	MY_FILTER_NAME,
	MY_FILTER_DESCRIPTION,
	MY_FILTER_CATEGORY,
	MY_FILTER_ENC_FMT,
	MY_FILTER_NINPUTS,
	MY_FILTER_NOUTPUTS,
	my_filter_init,
	my_filter_preprocess,
	my_filter_process,
	my_filter_postprocess,
	my_filter_uninit,
	my_filter_methods,
	MY_FILTER_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(my_filter_desc)
