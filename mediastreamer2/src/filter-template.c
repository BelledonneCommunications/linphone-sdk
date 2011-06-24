/*
 filter-template.m
 Copyright (C) 2011 Belledonne Communications, Grenoble, France
 
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
#include "msfilter.h"

/*filter common method*/
struct my_filter_struct {
    char toto;
};

static void filter_init(MSFilter *f){
        f->data = ms_new0(struct my_filter_struct,1);
}

static void filter_preprocess(MSFilter *f){
    
}

static void filter_process(MSFilter *f){
    
}

static void filter_postprocess(MSFilter *f){
    
}

static void filter_unit(MSFilter *f){
    
}


/*filter specific method*/

static int filter_set_sample_rate(MSFilter *f, void *arg) {
    return 0;
}

static int filter_get_sample_rate(MSFilter *f, void *arg) {
    return 0;
}

static MSFilterMethod filter_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE , filter_set_sample_rate },
    {	MS_FILTER_GET_SAMPLE_RATE , filter_get_sample_rate },
	{	0, NULL}
};



MSFilterDesc my_filter_desc={
	.id=MY_FILTER_ID, /* from Allfilters.h*/
	.name="MyFilter",
	.text="An empty filter.",
	.category=MS_FILTER_OTHER,
	.ninputs=1, /*number of inputs*/
	.noutputs=0, /*number of outputs*/
	.init=filter_init,
	.preprocess=filter_preprocess,
	.process=filter_process,
    .postprocess=filter_postprocess,
	.uninit=filter_unit,
	.methods=filter_methods
};