/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL, Grenoble France.

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

#include "layouts.h"


/* compute the ideal placement of the video within a window of size wsize,
given that the original video has size vsize. Put the result in rect*/

static void center_rectangle(MSVideoSize wsize, MSVideoSize vsize, MSRect *rect){
	int w,h;
	w=wsize.width & ~0x3;
	h=((w*vsize.height)/vsize.width) & ~0x1;
	if (h>wsize.height){
		/*the height doesn't fit, so compute the width*/
		h=wsize.height & ~0x1;
		w=((h*vsize.width)/vsize.height) & ~0x3;
	}
	rect->x=(wsize.width-w)/2;
	rect->y=(wsize.height-h)/2;
	rect->w=w;
	rect->h=h;
}


#define LOCAL_POS_OFFSET 10

/**
 * This function is used to compute placement of video and local preview video within a window.
 * It is used by display filters such as MSDrawDibDisplay and MSX11Video.
 * @arg wsize the size of the window
 * @arg vsize the size of the main video to display
 * @arg orig_psize the size of the preview video
 * @arg localrect_pos tells which corner is to be used to preview placement
 * @arg scalefactor ratio of the window size over the whished preview video size , ex: 4.0 
 * @arg mainrect is a return value describing the main video placement
 * @arg localrect is a return value describing the preview video placement
 * @arg localrect_pos
**/
void ms_layout_compute(MSVideoSize wsize, MSVideoSize vsize, MSVideoSize orig_psize,  int localrect_pos, float scalefactor, MSRect *mainrect, MSRect *localrect){
	MSVideoSize psize;

	center_rectangle(wsize,vsize,mainrect);
	if (localrect_pos!=-1){
		psize.width=(int)(wsize.width/scalefactor);
		psize.height=(int)(wsize.height/scalefactor);
		center_rectangle(psize,orig_psize,localrect);
		if ((wsize.height - mainrect->h < mainrect->h/scalefactor && wsize.width - mainrect->w < mainrect->w/scalefactor) || localrect_pos<=3)
		{
			int x_sv;
			int y_sv;
			if (localrect_pos%4==1)
			{
				/* top left corner */
				x_sv = LOCAL_POS_OFFSET;
				y_sv = LOCAL_POS_OFFSET;
			}
			else if (localrect_pos%4==2)
			{
				/* top right corner */
				x_sv = (wsize.width-localrect->w-LOCAL_POS_OFFSET);
				y_sv = LOCAL_POS_OFFSET;
			}
			else if (localrect_pos%4==3)
			{
				/* bottom left corner */
				x_sv = LOCAL_POS_OFFSET;
				y_sv = (wsize.height-localrect->h-LOCAL_POS_OFFSET);
			}
			else /* corner = 0: default */
			{
				/* bottom right corner */
				x_sv = (wsize.width-localrect->w-LOCAL_POS_OFFSET);
				y_sv = (wsize.height-localrect->h-LOCAL_POS_OFFSET);
			}
			localrect->x=x_sv; //wsize.width-localrect->w-LOCAL_POS_OFFSET;
			localrect->y=y_sv; //wsize.height-localrect->h-LOCAL_POS_OFFSET;
		}
		else
		{
			int x_sv;
			int y_sv;

			if (wsize.width - mainrect->w < mainrect->w/scalefactor)
			{
				// recalculate so we have a selfview taking as
				// much available space as possible
				psize.width=wsize.width;
				psize.height=wsize.height-mainrect->h;
				center_rectangle(psize,orig_psize,localrect);

				if (localrect_pos%4==1 || localrect_pos%4==2)
				{
					//Self View on Top
					x_sv = (wsize.width-localrect->w)/2;
					y_sv = LOCAL_POS_OFFSET;

					mainrect->y = wsize.height-mainrect->h-LOCAL_POS_OFFSET;
				}
				else
				{
					//Self View on Bottom
					x_sv = (wsize.width-localrect->w)/2;
					y_sv = (wsize.height-localrect->h-LOCAL_POS_OFFSET);

					mainrect->y = LOCAL_POS_OFFSET;
				}
			}
			else
			{
				// recalculate so we have a selfview taking as
				// much available space as possible
				psize.width=wsize.width-mainrect->w;
				psize.height=wsize.height;
				center_rectangle(psize,orig_psize,localrect);

				if (localrect_pos%4==1 || localrect_pos%4==3)
				{
					//Self View on left
					x_sv = LOCAL_POS_OFFSET;
					y_sv = (wsize.height-localrect->h)/2;

					mainrect->x = wsize.width-mainrect->w-LOCAL_POS_OFFSET;
				}
				else
				{
					//Self View on right
					x_sv = (wsize.width-localrect->w-LOCAL_POS_OFFSET);
					y_sv = (wsize.height-localrect->h)/2;

					mainrect->x = LOCAL_POS_OFFSET;
				}
			}

			localrect->x=x_sv; //wsize.width-localrect->w-LOCAL_POS_OFFSET;
			localrect->y=y_sv; //wsize.height-localrect->h-LOCAL_POS_OFFSET;
		}
	}
/*
	ms_message("Compute layout result for\nwindow size=%ix%i\nvideo orig size=%ix%i\nlocal size=%ix%i\nlocal orig size=%ix%i\n"
		"mainrect=%i,%i,%i,%i\tlocalrect=%i,%i,%i,%i",
		wsize.width,wsize.height,vsize.width,vsize.height,psize.width,psize.height,orig_psize.width,orig_psize.height,
		mainrect->x,mainrect->y,mainrect->w,mainrect->h,
		localrect->x,localrect->y,localrect->w,localrect->h);
*/
}
