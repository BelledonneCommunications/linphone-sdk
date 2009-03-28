/*
linphone, gtk-glade interface.
Copyright (C) 2008  Simon MORLAT (simon.morlat@linphone.org)

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

#include "linphone.h"

static void run_gtk(){
	while (gtk_events_pending ())
		gtk_main_iteration ();

}

void *linphone_gtk_wait(LinphoneCore *lc, void *ctx, LinphoneWaitingState ws, const char *purpose, float progress){
	GtkWidget *w;
	switch(ws){
		case LinphoneWaitingStart:
			gdk_threads_enter();
			w=linphone_gtk_create_window("waiting");
			gtk_window_set_transient_for(GTK_WINDOW(w),GTK_WINDOW(linphone_gtk_get_main_window()));
			gtk_window_set_position(GTK_WINDOW(w),GTK_WIN_POS_CENTER_ON_PARENT);
			if (purpose) {
				gtk_progress_bar_set_text(
					GTK_PROGRESS_BAR(linphone_gtk_get_widget(w,"progressbar")),
					purpose);
			}
			gtk_widget_show(w);
			/*g_message("Creating waiting window");*/
			run_gtk();
			gdk_threads_leave();
			return w;
		break;
		case LinphoneWaitingProgress:
			w=(GtkWidget*)ctx;
			gdk_threads_enter();
			if (progress>=0){
				gtk_progress_bar_set_fraction(
					GTK_PROGRESS_BAR(linphone_gtk_get_widget(w,"progressbar")),
					progress);
				
				
			}else {
				gtk_progress_bar_pulse(
					GTK_PROGRESS_BAR(linphone_gtk_get_widget(w,"progressbar"))
				);
			}
			/*g_message("Updating progress");*/
			run_gtk();
			gdk_threads_leave();
			g_usleep(50000);
			return w;
		break;
		case LinphoneWaitingFinished:
			w=(GtkWidget*)ctx;
			gdk_threads_enter();
			gtk_widget_destroy(w);
			run_gtk();
			gdk_threads_leave();
			return NULL;
		break;
	}
	return NULL;
}
