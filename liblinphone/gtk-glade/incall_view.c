/*
linphone, gtk-glade interface.
Copyright (C) 2009  Simon MORLAT (simon.morlat@linphone.org)

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
/*
*  C Implementation: incall_frame
*
* Description: 
*
*
* Author: Simon Morlat <simon.morlat@linphone.org>, (C) 2009
*
*
*/

#include "linphone.h"


gboolean linphone_gtk_use_in_call_view(){
	static int val=-1;
	if (val==-1) val=linphone_gtk_get_ui_config_int("use_incall_view",1);
	return val;
}

void linphone_gtk_show_in_call_view(void){
	GtkWidget *main_window=linphone_gtk_get_main_window();
	GtkNotebook *notebook=(GtkNotebook *)linphone_gtk_get_widget(main_window,"viewswitch");
	GtkWidget *in_call_frame=linphone_gtk_get_widget(main_window,"in_call_frame");
	gint idx;
	
	/* Make the in call frame visible and arrange for the notebook to
		 show that page */
	gtk_widget_show(in_call_frame);
	idx = gtk_notebook_page_num(notebook, in_call_frame);
	if (idx >= 0) {
		gtk_notebook_set_current_page(notebook, idx);
	}
}

void linphone_gtk_show_idle_view(void){
	GtkWidget *main_window=linphone_gtk_get_main_window();
	GtkNotebook *notebook=(GtkNotebook *)linphone_gtk_get_widget(main_window,"viewswitch");
	GtkWidget *idle_frame=linphone_gtk_get_widget(main_window,"idle_frame");
	GtkWidget *in_call_frame=linphone_gtk_get_widget(main_window,"in_call_frame");
	gint idx;

	/* Switch back to the idle frame page, maybe we should have
		 remembered where we were in gtk_show_in_call_view() to switch
		 back to that page of the notebook, but this should do in most
		 cases. */
	gtk_widget_show(idle_frame); /* Make sure it is visible... */
	idx = gtk_notebook_page_num(notebook, idle_frame);
	if (idx >= 0) {
		gtk_notebook_set_current_page(notebook, idx);
		gtk_widget_hide(in_call_frame);
	}
}

void display_peer_name_in_label(GtkWidget *label, const char *uri){
	LinphoneAddress *from;
	const char *displayname=NULL;
	char *id=NULL;
	char *uri_label;

	if (uri==NULL) {
		ms_error("Strange: in call with nobody ?");
		return;
	}

	from=linphone_address_new(uri);
	if (from!=NULL){
		
		if (linphone_address_get_display_name(from))
			displayname=linphone_address_get_display_name(from);

		id=linphone_address_as_string_uri_only(from);

	}else id=ms_strdup(uri);

	if (displayname!=NULL)
		uri_label=g_markup_printf_escaped("<span size=\"large\">%s</span>\n<i>%s</i>", 
			displayname,id);
	else
		uri_label=g_markup_printf_escaped("<span size=\"large\"><i>%s</i></span>\n",id);
	gtk_label_set_markup(GTK_LABEL(label),uri_label);
	ms_free(id);
	g_free(uri_label);
	if (from!=NULL) linphone_address_destroy(from);
}

void linphone_gtk_in_call_view_set_calling(const char *uri){
	GtkWidget *main_window=linphone_gtk_get_main_window();
	GtkWidget *status=linphone_gtk_get_widget(main_window,"in_call_status");
	GtkWidget *callee=linphone_gtk_get_widget(main_window,"in_call_uri");
	GtkWidget *duration=linphone_gtk_get_widget(main_window,"in_call_duration");
	GtkWidget *animation=linphone_gtk_get_widget(main_window,"in_call_animation");
	GdkPixbufAnimation *pbuf=create_pixbuf_animation("calling_anim.gif");

	gtk_label_set_markup(GTK_LABEL(status),_("<b>Calling...</b>"));
	display_peer_name_in_label(callee,uri);
	
	gtk_label_set_text(GTK_LABEL(duration),_("00::00::00"));
	if (pbuf!=NULL){
		gtk_image_set_from_animation(GTK_IMAGE(animation),pbuf);
		g_object_unref(G_OBJECT(pbuf));
	}else gtk_image_set_from_stock(GTK_IMAGE(animation),GTK_STOCK_INFO,GTK_ICON_SIZE_DIALOG);
}

void linphone_gtk_in_call_view_set_in_call(){
	LinphoneCore *lc=linphone_gtk_get_core();
	GtkWidget *main_window=linphone_gtk_get_main_window();
	GtkWidget *status=linphone_gtk_get_widget(main_window,"in_call_status");
	GtkWidget *callee=linphone_gtk_get_widget(main_window,"in_call_uri");
	GtkWidget *duration=linphone_gtk_get_widget(main_window,"in_call_duration");
	GtkWidget *animation=linphone_gtk_get_widget(main_window,"in_call_animation");
	GdkPixbufAnimation *pbuf=create_pixbuf_animation("incall_anim.gif");
	const LinphoneAddress *uri=linphone_core_get_remote_uri(lc);
	char *tmp=linphone_address_as_string(uri);
	display_peer_name_in_label(callee,tmp);
	ms_free(tmp);

	gtk_label_set_markup(GTK_LABEL(status),_("<b>In call with</b>"));

	gtk_label_set_text(GTK_LABEL(duration),_("00::00::00"));
	if (pbuf!=NULL){
		gtk_image_set_from_animation(GTK_IMAGE(animation),pbuf);
		g_object_unref(G_OBJECT(pbuf));
	}else gtk_image_set_from_stock(GTK_IMAGE(animation),GTK_STOCK_INFO,GTK_ICON_SIZE_DIALOG);
	linphone_gtk_enable_mute_button(
		GTK_TOGGLE_BUTTON(linphone_gtk_get_widget(main_window,"incall_mute")),TRUE);
}

void linphone_gtk_in_call_view_update_duration(int duration){
	GtkWidget *main_window=linphone_gtk_get_main_window();
	GtkWidget *duration_label=linphone_gtk_get_widget(main_window,"in_call_duration");
	char tmp[256]={0};
	int seconds=duration%60;
	int minutes=(duration/60)%60;
	int hours=duration/3600;
	snprintf(tmp,sizeof(tmp)-1,_("%02i::%02i::%02i"),hours,minutes,seconds);
	gtk_label_set_text(GTK_LABEL(duration_label),tmp);
}

static gboolean in_call_view_terminated(){
	linphone_gtk_show_idle_view();
	return FALSE;
}

void linphone_gtk_in_call_view_terminate(const char *error_msg){
	GtkWidget *main_window=linphone_gtk_get_main_window();
	GtkWidget *status=linphone_gtk_get_widget(main_window,"in_call_status");
	GtkWidget *animation=linphone_gtk_get_widget(main_window,"in_call_animation");
	GdkPixbuf *pbuf=create_pixbuf(linphone_gtk_get_ui_config("stop_call_icon","stopcall-red.png"));

	if (error_msg==NULL)
		gtk_label_set_markup(GTK_LABEL(status),_("<b>Call ended.</b>"));
	else{
		char *msg=g_markup_printf_escaped("<span color=\"red\"><b>%s</b></span>",error_msg);
		gtk_label_set_markup(GTK_LABEL(status),msg);
		g_free(msg);
	}
	if (pbuf!=NULL){
		gtk_image_set_from_pixbuf(GTK_IMAGE(animation),pbuf);
		g_object_unref(G_OBJECT(pbuf));
	}
	linphone_gtk_enable_mute_button(
		GTK_TOGGLE_BUTTON(linphone_gtk_get_widget(main_window,"incall_mute")),FALSE);
	g_timeout_add_seconds(2,(GSourceFunc)in_call_view_terminated,NULL);
}

void linphone_gtk_draw_mute_button(GtkToggleButton *button, gboolean active){
	if (active){
		GtkWidget *image=create_pixmap("mic_muted.png");
		gtk_button_set_label(GTK_BUTTON(button),_("Unmute"));
		if (image!=NULL) {
			gtk_button_set_image(GTK_BUTTON(button),image);
			gtk_widget_show(image);
		}
	}else{
		GtkWidget *image=create_pixmap("mic_active.png");
		gtk_button_set_label(GTK_BUTTON(button),_("Mute"));
		if (image!=NULL) {
			gtk_button_set_image(GTK_BUTTON(button),image);
			gtk_widget_show(image);
		}
	}
}

void linphone_gtk_mute_toggled(GtkToggleButton *button){
	gboolean active=gtk_toggle_button_get_active(button);
	linphone_core_mute_mic(linphone_gtk_get_core(),active);
	linphone_gtk_draw_mute_button(button,active);
}

void linphone_gtk_enable_mute_button(GtkToggleButton *button, gboolean sensitive){
	gtk_widget_set_sensitive(GTK_WIDGET(button),sensitive);
	linphone_gtk_draw_mute_button(button,FALSE);
}
