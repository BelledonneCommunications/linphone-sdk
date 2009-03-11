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

#include <gtk/gtk.h>
#include "linphone.h"

static GtkWidget *linphone_gtk_create_contact_menu(GtkWidget *contact_list);

enum{
	FRIEND_PRESENCE_IMG,
	FRIEND_NAME,
	FRIEND_PRESENCE_STATUS,
	FRIEND_ID,
	FRIEND_SIP_ADDRESS,
	FRIEND_LIST_NCOL
};


typedef struct _status_picture_tab_t{
	LinphoneOnlineStatus status;
	const char *img;
} status_picture_tab_t;

status_picture_tab_t status_picture_tab[]={
	{	LINPHONE_STATUS_UNKNOWN,	"sip-closed.png"	},
	{	LINPHONE_STATUS_ONLINE,		"sip-online.png"	},
	{	LINPHONE_STATUS_BUSY,		"sip-busy.png"		},
	{	LINPHONE_STATUS_BERIGHTBACK,	"sip-bifm.png"		},
	{	LINPHONE_STATUS_AWAY,		"sip-away.png"		},
	{	LINPHONE_STATUS_ONTHEPHONE,	"sip-otp.png"		},
	{	LINPHONE_STATUS_OUTTOLUNCH,	"sip-otl.png"		},
	{	LINPHONE_STATUS_NOT_DISTURB,	"sip-closed.png"	},
	{	LINPHONE_STATUS_MOVED,		"sip-closed.png"	},
	{	LINPHONE_STATUS_ALT_SERVICE,	"sip-closed.png"	},
	{	LINPHONE_STATUS_OFFLINE,	"sip-away.png"		},
	{	LINPHONE_STATUS_PENDING,	"sip-wfa.png"		},
	{	LINPHONE_STATUS_CLOSED,		"sip-closed.png"	},
	{	LINPHONE_STATUS_END,		NULL			},
};

static GdkPixbuf *create_status_picture(LinphoneOnlineStatus ss){
	status_picture_tab_t *t=status_picture_tab;
	while(t->img!=NULL){
		if (ss==t->status){
			GdkPixbuf *pixbuf;
			pixbuf = create_pixbuf(t->img);
			return pixbuf;
		}
		++t;
	}
	g_error("No pixmap defined for status %i",ss);
	return NULL;
}

void linphone_gtk_set_friend_status(GtkWidget *friendlist , LinphoneFriend * fid, const gchar *url, const gchar *status, const gchar *img){
	GtkTreeIter iter;
	LinphoneFriend *tmp=0;
	gboolean found=FALSE;
	GtkTreeModel *model=gtk_tree_view_get_model(GTK_TREE_VIEW(friendlist));
	if (gtk_tree_model_get_iter_first(model,&iter)) {
		do{
			gtk_tree_model_get(model,&iter,FRIEND_ID,&tmp,-1);
			//printf("tmp=%i, fid=%i",tmp,fid);
			if (fid==tmp) {
				GdkPixbuf *pixbuf;
				gtk_list_store_set(GTK_LIST_STORE(model),&iter,FRIEND_PRESENCE_STATUS,status,-1);
				pixbuf = create_pixbuf(img);
				if (pixbuf)
				  {
				    gtk_list_store_set(GTK_LIST_STORE(model),&iter,FRIEND_PRESENCE_IMG, pixbuf,-1);
				  }
				  found=TRUE;
			}
		}while(gtk_tree_model_iter_next(model,&iter));
	}
	
}


static void linphone_gtk_set_selection_to_uri_bar(GtkTreeView *treeview){
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *model;
	LinphoneFriend *lf=NULL;
	gchar* friend;
	select = gtk_tree_view_get_selection (treeview);
	if (gtk_tree_selection_get_selected (select, &model, &iter))
	{
		gtk_tree_model_get (model, &iter,FRIEND_ID , &lf, -1);
		friend=linphone_friend_get_url(lf);
		gtk_entry_set_text(GTK_ENTRY(linphone_gtk_get_widget(linphone_gtk_get_main_window(),"uribar")),friend);
		ms_free(friend);
	}
}

static void linphone_gtk_call_selected(GtkTreeView *treeview){
	linphone_gtk_set_selection_to_uri_bar(treeview);
	linphone_gtk_start_call(linphone_gtk_get_widget(gtk_widget_get_toplevel(GTK_WIDGET(treeview)),
					"start_call"));
}

void linphone_gtk_contact_activated(GtkTreeView     *treeview,
                                    GtkTreePath     *path,
                                    GtkTreeViewColumn *column,
                                        gpointer         user_data)
{
	linphone_gtk_call_selected(treeview);
}

void linphone_gtk_contact_clicked(GtkTreeView     *treeview){
	linphone_gtk_set_selection_to_uri_bar(treeview);
}

static GtkWidget * create_presence_menu(){
	GtkWidget *menu=gtk_menu_new();
	GtkWidget *menu_item;
	GdkPixbuf *pbuf;
	status_picture_tab_t *t;
	for(t=status_picture_tab;t->img!=NULL;++t){
		if (t->status==LINPHONE_STATUS_UNKNOWN ||
			t->status==LINPHONE_STATUS_PENDING){
			continue;
		}
		menu_item=gtk_image_menu_item_new_with_label(linphone_online_status_to_string(t->status));
		pbuf=create_status_picture(t->status);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
						gtk_image_new_from_pixbuf(pbuf));
		g_object_unref(G_OBJECT(pbuf));
		gtk_widget_show(menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
		g_signal_connect_swapped(G_OBJECT(menu_item),"activate",(GCallback)linphone_gtk_set_my_presence,GINT_TO_POINTER(t->status));
	}
	return menu;
}

void linphone_gtk_set_my_presence(LinphoneOnlineStatus ss){
	GtkWidget *button=linphone_gtk_get_widget(linphone_gtk_get_main_window(),"presence_button");
	GdkPixbuf *pbuf=create_status_picture(ss);
	GtkWidget *image=gtk_image_new_from_pixbuf(pbuf);
	GtkWidget *menu;
	g_object_unref(G_OBJECT(pbuf));
	gtk_button_set_label(GTK_BUTTON(button),linphone_online_status_to_string(ss));
	gtk_button_set_image(GTK_BUTTON(button),image);
	/*prepare menu*/
	menu=(GtkWidget*)g_object_get_data(G_OBJECT(button),"presence_menu");
	if (menu==NULL){
		menu=create_presence_menu();
		/*the menu is destroyed when the button is destroyed*/
		g_object_weak_ref(G_OBJECT(button),(GWeakNotify)gtk_widget_destroy,menu);
		g_object_set_data(G_OBJECT(button),"presence_menu",menu);
	}
	linphone_core_set_presence_info(linphone_gtk_get_core(),0,NULL,ss);
}

void linphone_gtk_my_presence_clicked(GtkWidget *button){
	GtkWidget *menu=(GtkWidget*)g_object_get_data(G_OBJECT(button),"presence_menu");
	gtk_menu_popup(GTK_MENU(menu),NULL,NULL,NULL,NULL,0,
			gtk_get_current_event_time());
	gtk_widget_show(menu);
}


static void linphone_gtk_friend_list_init(GtkWidget *friendlist)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *select;
	
	
	store = gtk_list_store_new(FRIEND_LIST_NCOL, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,  G_TYPE_POINTER,
					G_TYPE_STRING);
	/* need to add friends to the store here ...*/
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(friendlist),GTK_TREE_MODEL(store));
	g_object_unref(G_OBJECT(store));

	renderer = gtk_cell_renderer_pixbuf_new();
	column = gtk_tree_view_column_new_with_attributes (NULL,
                                                   renderer,
                                                   "pixbuf", FRIEND_PRESENCE_IMG,
                                                   NULL);
	gtk_tree_view_column_set_min_width (column, 29);
	gtk_tree_view_append_column (GTK_TREE_VIEW (friendlist), column);

	gtk_tree_view_column_set_visible(column,linphone_gtk_get_ui_config_int("friendlist_icon",1));

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Name"),
                                                   renderer,
                                                   "text", FRIEND_NAME,
                                                   NULL);
	g_object_set (G_OBJECT(column), "resizable", TRUE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (friendlist), column);

	column = gtk_tree_view_column_new_with_attributes (_("Presence status"),
                                                   renderer,
                                                   "text", FRIEND_PRESENCE_STATUS,
                                                   NULL);
	g_object_set (G_OBJECT(column), "resizable", TRUE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (friendlist), column);
	gtk_tree_view_column_set_visible(column,linphone_gtk_get_ui_config_int("friendlist_status",1));
	
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (friendlist));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(friendlist),FRIEND_SIP_ADDRESS);

	gtk_combo_box_set_active(GTK_COMBO_BOX(linphone_gtk_get_widget(
					gtk_widget_get_toplevel(friendlist),"show_category")),0);
}

void linphone_gtk_show_friends(void){
	GtkWidget *mw=linphone_gtk_get_main_window();
	GtkWidget *friendlist=linphone_gtk_get_widget(mw,"contact_list");
	GtkListStore *store=NULL;
	GtkTreeIter iter;
	const MSList *itf;
	GtkWidget *category=linphone_gtk_get_widget(mw,"show_category");
	GtkWidget *filter=linphone_gtk_get_widget(mw,"search_bar");
	LinphoneCore *core=linphone_gtk_get_core();
	const gchar *search=NULL;
	gboolean online_only=FALSE,lookup=FALSE;

	if (gtk_tree_view_get_model(GTK_TREE_VIEW(friendlist))==NULL){
		linphone_gtk_friend_list_init(friendlist);
	}
	store=GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(friendlist)));
	gtk_list_store_clear(store);

	online_only=(gtk_combo_box_get_active(GTK_COMBO_BOX(category))==1);
	search=gtk_entry_get_text(GTK_ENTRY(filter));
	if (search==NULL || search[0]=='\0')
		lookup=FALSE;
	else lookup=TRUE;

	for(itf=linphone_core_get_friend_list(core);itf!=NULL;itf=ms_list_next(itf)){
		LinphoneFriend *lf=(LinphoneFriend*)itf->data;
		char *uri=linphone_friend_get_url(lf);
		char *name=linphone_friend_get_name(lf);
		char *addr=linphone_friend_get_addr(lf);
		char *display=name;
		char *escaped=NULL;
		if (lookup){
			if (strstr(uri,search)==NULL){
				ms_free(uri);
				if (name) ms_free(name);
				if (addr) ms_free(addr);
				continue;
			}
		}
		if (!online_only || (linphone_friend_get_status(lf)!=LINPHONE_STATUS_OFFLINE)){
			if (name==NULL || name[0]=='\0') display=addr;
			gtk_list_store_append(store,&iter);
			gtk_list_store_set(store,&iter,FRIEND_NAME, display,
					FRIEND_PRESENCE_STATUS, linphone_online_status_to_string(linphone_friend_get_status(lf)),
					FRIEND_ID,lf,-1);
			gtk_list_store_set(store,&iter,
				FRIEND_PRESENCE_IMG, create_status_picture(linphone_friend_get_status(lf)),
				-1);
			escaped=g_markup_escape_text(uri,-1);
			gtk_list_store_set(store,&iter,FRIEND_SIP_ADDRESS,escaped,-1);
			g_free(escaped);
		}
		ms_free(uri);
		if (name) ms_free(name);
		if (addr) ms_free(addr);
	}
}

void linphone_gtk_add_contact(void){
	GtkWidget *w=linphone_gtk_create_window("contact");
	gtk_widget_show(w);
}

void linphone_gtk_remove_contact(GtkWidget *button){
	GtkWidget *w=gtk_widget_get_toplevel(button);
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *model;
	LinphoneFriend *lf=NULL;
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(linphone_gtk_get_widget(w,"contact_list")));
	if (gtk_tree_selection_get_selected (select, &model, &iter))
	{
		gtk_tree_model_get (model, &iter,FRIEND_ID , &lf, -1);
		linphone_core_remove_friend(linphone_gtk_get_core(),lf);
		linphone_gtk_show_friends();
	}
}

void linphone_gtk_show_contact(LinphoneFriend *lf){
	GtkWidget *w=linphone_gtk_create_window("contact");
	char *uri,*name;
	uri=linphone_friend_get_addr(lf);
	name=linphone_friend_get_name(lf);
	if (uri) {
		gtk_entry_set_text(GTK_ENTRY(linphone_gtk_get_widget(w,"sip_address")),uri);
		ms_free(uri);
	}
	if (name){
		gtk_entry_set_text(GTK_ENTRY(linphone_gtk_get_widget(w,"name")),name);
		ms_free(name);
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(linphone_gtk_get_widget(w,"show_presence")),
					linphone_friend_get_send_subscribe(lf));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(linphone_gtk_get_widget(w,"allow_presence")),
					linphone_friend_get_inc_subscribe_policy(lf)==LinphoneSPAccept);
	g_object_set_data(G_OBJECT(w),"friend_ref",(gpointer)lf);
	gtk_widget_show(w);
}

void linphone_gtk_edit_contact(GtkWidget *button){
	GtkWidget *w=gtk_widget_get_toplevel(button);
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *model;
	LinphoneFriend *lf=NULL;
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(linphone_gtk_get_widget(w,"contact_list")));
	if (gtk_tree_selection_get_selected (select, &model, &iter))
	{
		gtk_tree_model_get (model, &iter,FRIEND_ID , &lf, -1);
		linphone_gtk_show_contact(lf);
	}
}

void linphone_gtk_chat_selected(GtkWidget *item){
	GtkWidget *w=gtk_widget_get_toplevel(item);
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *model;
	LinphoneFriend *lf=NULL;
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(linphone_gtk_get_widget(w,"contact_list")));
	if (gtk_tree_selection_get_selected (select, &model, &iter))
	{
		char *uri;
		gtk_tree_model_get (model, &iter,FRIEND_ID , &lf, -1);
		uri=linphone_friend_get_url(lf);
		linphone_gtk_create_chatroom(uri);
		ms_free(uri);
	}
}

void linphone_gtk_contact_cancel(GtkWidget *button){
	gtk_widget_destroy(gtk_widget_get_toplevel(button));
}

void linphone_gtk_contact_ok(GtkWidget *button){
	GtkWidget *w=gtk_widget_get_toplevel(button);
	LinphoneFriend *lf=(LinphoneFriend*)g_object_get_data(G_OBJECT(w),"friend_ref");
	char *fixed_uri=NULL;
	gboolean show_presence,allow_presence;
	const gchar *name,*uri;
	if (lf==NULL){
		lf=linphone_friend_new();
	}
	name=gtk_entry_get_text(GTK_ENTRY(linphone_gtk_get_widget(w,"name")));
	uri=gtk_entry_get_text(GTK_ENTRY(linphone_gtk_get_widget(w,"sip_address")));
	show_presence=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(linphone_gtk_get_widget(w,"show_presence")));
	allow_presence=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(linphone_gtk_get_widget(w,"allow_presence")));
	linphone_core_interpret_friend_uri(linphone_gtk_get_core(),uri,&fixed_uri);
	if (fixed_uri==NULL){
		linphone_gtk_display_something(GTK_MESSAGE_WARNING,_("Invalid sip contact !"));
		return ;
	}
	linphone_friend_set_sip_addr(lf,fixed_uri);
	ms_free(fixed_uri);
	linphone_friend_set_name(lf,name);
	linphone_friend_send_subscribe(lf,show_presence);
	linphone_friend_set_inc_subscribe_policy(lf,allow_presence==TRUE ? LinphoneSPAccept : LinphoneSPDeny);
	if (linphone_friend_in_list(lf)) {
		linphone_friend_done(lf);
	}else{
		linphone_core_add_friend(linphone_gtk_get_core(),lf);
	}
	linphone_gtk_show_friends();
	gtk_widget_destroy(w);
}

static GtkWidget *linphone_gtk_create_contact_menu(GtkWidget *contact_list){
	GtkWidget *menu=gtk_menu_new();
	GtkWidget *menu_item;
	gchar *call_label=NULL;
	gchar *text_label=NULL;
	gchar *edit_label=NULL;
	gchar *delete_label=NULL;
	gchar *name=NULL;
	GtkTreeSelection *select;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkWidget *image;
	LinphoneCore *lc=linphone_gtk_get_core();
	LinphoneProxyConfig *cfg=NULL;
	SipSetupContext * ssc=NULL;

	linphone_core_get_default_proxy(lc,&cfg);
	if (cfg){
		ssc=linphone_proxy_config_get_sip_setup_context(cfg);
	}

	g_signal_connect(G_OBJECT(menu), "selection-done", G_CALLBACK (gtk_widget_destroy), NULL);
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(contact_list));
	if (gtk_tree_selection_get_selected (select, &model, &iter)){
		gtk_tree_model_get(model, &iter,FRIEND_NAME , &name, -1);
		call_label=g_strdup_printf(_("Call %s"),name);
		text_label=g_strdup_printf(_("Send text to %s"),name);
		edit_label=g_strdup_printf(_("Edit contact '%s'"),name);
		delete_label=g_strdup_printf(_("Delete contact '%s'"),name);
		g_free(name);
	}
	if (call_label){
		menu_item=gtk_image_menu_item_new_with_label(call_label);
		image=gtk_image_new_from_stock(GTK_STOCK_NETWORK,GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),image);
		gtk_widget_show(image);
		gtk_widget_show(menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
		g_signal_connect_swapped(G_OBJECT(menu_item),"activate",(GCallback)linphone_gtk_call_selected,contact_list);
	}
	if (text_label){
		menu_item=gtk_image_menu_item_new_with_label(text_label);
		image=gtk_image_new_from_stock(GTK_STOCK_NETWORK,GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),image);
		gtk_widget_show(image);
		gtk_widget_show(menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
		g_signal_connect_swapped(G_OBJECT(menu_item),"activate",(GCallback)linphone_gtk_chat_selected,contact_list);
	}
	if (edit_label){
		menu_item=gtk_image_menu_item_new_with_label(edit_label);
		image=gtk_image_new_from_stock(GTK_STOCK_EDIT,GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),image);
		gtk_widget_show(image);
		gtk_widget_show(menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
		g_signal_connect_swapped(G_OBJECT(menu_item),"activate",(GCallback)linphone_gtk_edit_contact,contact_list);
	}
	if (delete_label){
		menu_item=gtk_image_menu_item_new_with_label(delete_label);
		image=gtk_image_new_from_stock(GTK_STOCK_DELETE,GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),image);
		gtk_widget_show(image);
		gtk_widget_show(menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
		g_signal_connect_swapped(G_OBJECT(menu_item),"activate",(GCallback)linphone_gtk_remove_contact,contact_list);
	}
	

	if (ssc && (sip_setup_context_get_capabilities(ssc) & SIP_SETUP_CAP_BUDDY_LOOKUP)) {
		gchar *tmp=g_strdup_printf(_("Add new contact from %s directory"),linphone_proxy_config_get_domain(cfg));
		menu_item=gtk_image_menu_item_new_with_label(tmp);
		g_free(tmp);
		image=gtk_image_new_from_stock(GTK_STOCK_ADD,GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),image);
		gtk_widget_show(image);
		gtk_widget_show(menu_item);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
		g_signal_connect_swapped(G_OBJECT(menu_item),"activate",(GCallback)linphone_gtk_show_buddy_lookup_window,ssc);
		gtk_widget_show(menu);
	}
	
	menu_item=gtk_image_menu_item_new_from_stock(GTK_STOCK_ADD,NULL);
	gtk_widget_show(menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu),menu_item);
	g_signal_connect_swapped(G_OBJECT(menu_item),"activate",(GCallback)linphone_gtk_add_contact,contact_list);
	gtk_widget_show(menu);
	gtk_menu_attach_to_widget (GTK_MENU (menu), contact_list, NULL);

	if (call_label) g_free(call_label);
	if (text_label) g_free(text_label);
	if (edit_label) g_free(edit_label);
	if (delete_label) g_free(delete_label);
	return menu;
}


gboolean linphone_gtk_popup_contact_menu(GtkWidget *list, GdkEventButton *event){
	GtkWidget *m=linphone_gtk_create_contact_menu(list);
	gtk_menu_popup (GTK_MENU (m), NULL, NULL, NULL, NULL, 
                  event ? event->button : 0, event ? event->time : gtk_get_current_event_time());
	return TRUE;
}

gboolean linphone_gtk_contact_list_button_pressed(GtkWidget *widget, GdkEventButton *event){
	/* Ignore double-clicks and triple-clicks */
	if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
	{
		return linphone_gtk_popup_contact_menu(widget, event);
	}
	return FALSE;
}

