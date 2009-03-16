#include "linphone.h"

#include "lpconfig.h"

static GList *pixmaps_directories = NULL;

/* Use this function to set the directory containing installed pixmaps. */
void
add_pixmap_directory                   (const gchar     *directory)
{
  pixmaps_directories = g_list_prepend (pixmaps_directories,
                                        g_strdup (directory));
}

/* This is an internally used function to find pixmap files. */
static gchar*
find_pixmap_file                       (const gchar     *filename)
{
  GList *elem;

  /* We step through each of the pixmaps directory to find it. */
  elem = pixmaps_directories;
  while (elem)
    {
      gchar *pathname = g_strdup_printf ("%s%s%s", (gchar*)elem->data,
                                         G_DIR_SEPARATOR_S, filename);
      if (g_file_test (pathname, G_FILE_TEST_EXISTS))
        return pathname;
      g_free (pathname);
      elem = elem->next;
    }
  return NULL;
}

/* This is an internally used function to create pixmaps. */
GtkWidget*
create_pixmap                          (GtkWidget       *widget,
                                        const gchar     *filename)
{
  gchar *pathname = NULL;
  GtkWidget *pixmap;

  if (!filename || !filename[0])
      return gtk_image_new ();

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning (_("Couldn't find pixmap file: %s"), filename);
      return gtk_image_new ();
    }

  pixmap = gtk_image_new_from_file (pathname);
  g_free (pathname);
  return pixmap;
}

/* This is an internally used function to create pixmaps. */
GdkPixbuf*
create_pixbuf                          (const gchar     *filename)
{
  gchar *pathname = NULL;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (!filename || !filename[0])
      return NULL;

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning (_("Couldn't find pixmap file: %s"), filename);
      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
  if (!pixbuf)
    {
      fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
               pathname, error->message);
      g_error_free (error);
    }
  g_free (pathname);
  return pixbuf;
}

/* This is used to set ATK action descriptions. */
void
glade_set_atk_action_description       (AtkAction       *action,
                                        const gchar     *action_name,
                                        const gchar     *description)
{
  gint n_actions, i;

  n_actions = atk_action_get_n_actions (action);
  for (i = 0; i < n_actions; i++)
    {
      if (!strcmp (atk_action_get_name (action, i), action_name))
        atk_action_set_description (action, i, description);
    }
}


static char linphone_lang[256]={0};

/*lang has to be read before the config file is parsed...*/
const char *linphone_gtk_get_lang(const char *config_file){
	FILE *f=fopen(config_file,"r");
	if (f){
		char tmp[256];
		while(fgets(tmp,sizeof(tmp),f)!=NULL){
			char *p;
			if ((p=strstr(tmp,"lang="))!=NULL){
				p+=5;
				sscanf(p,"%s",linphone_lang);
				g_message("Found lang %s",linphone_lang);
				break;
			}
		}
		fclose(f);
	}
	return linphone_lang;
}

const gchar *linphone_gtk_get_ui_config(const char *key, const char *def){
	LinphoneCore *lc=linphone_gtk_get_core();
	if (lc){
		LpConfig *cfg=linphone_core_get_config(linphone_gtk_get_core());
		return lp_config_get_string(cfg,"GtkUi",key,def);
	}else{
		ms_warning ("Cannot read config, no core created yet.");
		return NULL;
	}
}

int linphone_gtk_get_ui_config_int(const char *key, int def){
	LpConfig *cfg=linphone_core_get_config(linphone_gtk_get_core());
	return lp_config_get_int(cfg,"GtkUi",key,def);
}


