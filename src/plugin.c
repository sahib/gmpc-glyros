/* gmpc-glyros (GMPC plugin)
 * Copyright (C) 2011 
 * Project homepage: 

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glyr/glyr.h>
#include <gtk/gtk.h>
#include <gmpc/plugin.h>
#include <gmpc/metadata.h>

#include "config.h"

#define LOG_DOMAIN "Gmpc.Provider.Glyros"

#define LOG_SUBCLASS        "glyros"
#define LOG_COVER_NAME      "fetch-art-album"
#define LOG_ARTIST_ART      "fetch-art-artist"
#define LOG_SIMILIAR_ARTIST "fetch-similiar-artist"
#define LOG_SIMILIAR_SONG   "fetch-similiar-song"
#define LOG_SIMILIAR_GENRE  "fetch-similiar-genre"
#define LOG_ARTIST_TXT      "fetch-biography-artist"
#define LOG_SONG_TXT        "fetch-lyrics"
#define LOG_ALBUM_TXT       "fetch-album-txt"


gmpcPlugin glyros_plugin;

/* API Version. Needed. */
int plugin_api_version = PLUGIN_API_VERSION;

static void glyros_init(void)
{
	Gly_init();
	atexit(Gly_cleanup);
}
static struct glyros_fetch_thread_data
{
	mpd_Song *song;
	MetaDataType type;
	void (*callback)(GList *list, gpointer data);
	gpointer user_data;
} glyros_fetch_thread_data;

static int glyros_fetch_cover_priority(void)
{
	return cfg_get_single_value_as_int_with_default(config, LOG_SUBCLASS, "priority", 20);
}
static void glyros_fetch_cover_priority_set(int priority)
{
	cfg_set_single_value_as_int(config, LOG_SUBCLASS, "priority", priority);
}
static int glyros_get_enabled(void)
{
	return cfg_get_single_value_as_int_with_default(config, LOG_SUBCLASS, "enable", TRUE);
}
static void glyros_set_enabled(int enabled)
{
	cfg_set_single_value_as_int(config, LOG_SUBCLASS, "enable", enabled);
}

static glyros_set_proxy(GlyQuery * q)
{
/*
	[Network Settings]
	Use Proxy="1"
	Proxy Port="8080"
	Use authentication="1"
	Proxy Address="http://Proxy-fh-hof.de"
	Proxy authentication username="aegsg"
	Proxy authentication password="egedhh"
	[]

	=> [protocol://][user:pass@]Domain[:port]
*/
	if(q != NULL)
	{
		const char * sublcass = "Network Settings";
		if(cfg_get_single_value_as_int_with_default(config,sublcass,"Use Proxy",FALSE)) 
		{
			char * port = cfg_get_single_value_as_string_with_default(config,sublcass,"Proxy Port","8080");
			char * addr = cfg_get_single_value_as_string_with_default(config,sublcass,"Proxy Address","localhost");
	
			char * user = "", * passwd = "";
			if(cfg_get_single_value_as_int_with_default(config,sublcass,"Use Proxy",FALSE)) 
			{ 
				user   = cfg_get_single_value_as_string_with_default(config,sublcass,"Proxy authentication username","");
				passwd = cfg_get_single_value_as_string_with_default(config,sublcass,"Proxy authentication password","");
			}

			if(port != NULL && addr != NULL) 
			{
				/* remove protocol if present , it's not used. */
				char * proto;
				if((proto = strstr(addr,"://"))) 
				{
					proto += 3;
					addr = proto;
				}

				char * proxystring = g_strdup_printf("%s:%s@%s:%s",user,passwd,addr,port);
				if(proxystring != NULL)
				{
					GlyOpt_proxy(q,proxystring);
					puts(proxystring);
					free(proxystring);
				}
			}
		}
	}	
}
static gpointer glyros_fetch_thread(void * data)
{
	/* arguments */
	struct glyros_fetch_thread_data * thread_data = data;

	/* cache */
	GlyMemCache * cache = NULL;

	/* query */
	GlyQuery q;

	/* data type */
	MetaDataContentType content_type = META_DATA_CONTENT_RAW;

	/* query init */
	Gly_init_query(&q);

	/* set metadata */
	GlyOpt_artist(&q,(char*)thread_data->song->artist);
	GlyOpt_album (&q,(char*)thread_data->song->album);
	GlyOpt_title (&q,(char*)thread_data->song->title);

	/* Set proxy */
	glyros_set_proxy(&q);

	/* set default type */
	GlyOpt_type(&q, GET_UNSURE);

	/* set type */
	if (glyros_get_enabled() == TRUE) 
	{
		if (thread_data->song->artist != NULL)
		{
			if (thread_data->type == META_ARTIST_ART &&
			    cfg_get_single_value_as_int_with_default(config,LOG_SUBCLASS, LOG_ARTIST_ART,TRUE))
			{
				GlyOpt_type(&q, GET_ARTIST_PHOTOS);
				content_type = META_DATA_CONTENT_RAW;
			}
			else if (thread_data->type == META_ARTIST_TXT &&
				 cfg_get_single_value_as_int_with_default(config,LOG_SUBCLASS,LOG_ARTIST_TXT,TRUE))
			{
				GlyOpt_type(&q, GET_ARTISTBIO);
				content_type = META_DATA_CONTENT_TEXT;
			}
			else if (thread_data->type == META_ARTIST_SIMILAR &&
				 cfg_get_single_value_as_int_with_default(config,LOG_SUBCLASS,LOG_SIMILIAR_ARTIST,TRUE)) 
			{
				GlyOpt_type(&q, GET_SIMILIAR_ARTISTS);
				content_type = META_DATA_CONTENT_TEXT;
			}
			else if (thread_data->type == META_ALBUM_ART &&
				 thread_data->song->album != NULL    &&
				 cfg_get_single_value_as_int_with_default(config,LOG_SUBCLASS,LOG_COVER_NAME,TRUE))
			{
				GlyOpt_type(&q, GET_COVERART);
				content_type = META_DATA_CONTENT_RAW;
			}
			else if (thread_data->type == META_ALBUM_TXT &&
				 thread_data->song->album != NULL    &&
				 cfg_get_single_value_as_int_with_default(config,LOG_SUBCLASS,LOG_ALBUM_TXT,TRUE)) //---------
			{
				GlyOpt_type(&q, GET_ALBUM_REVIEW);
				content_type = META_DATA_CONTENT_TEXT;
			}
			else if (thread_data->type == META_SONG_TXT &&
				 thread_data->song->title != NULL   &&
				 cfg_get_single_value_as_int_with_default(config,LOG_SUBCLASS,LOG_SONG_TXT,TRUE)) 
			{
				GlyOpt_type(&q, GET_LYRICS);
				content_type = META_DATA_CONTENT_TEXT;
			}
			else if (thread_data->type == META_SONG_SIMILAR && 
				 thread_data->song->title != NULL       &&
				 cfg_get_single_value_as_int_with_default(config,LOG_SUBCLASS,LOG_SIMILIAR_SONG,TRUE))
			{
				/* not yet supported */
			}
			else if (thread_data->type == META_SONG_GUITAR_TAB && thread_data->song->title != NULL)
			{
				/* not supported */
			}
		}
		else if (thread_data->song->genre != NULL)
		{
			if (thread_data->type == META_GENRE_SIMILAR)
			{	
				/* not supported */
			}
		}
	}

	/* debug info */
	printf(
			"Artist: %s\n"
			"Title: %s\n"
			"Album: %s\n", 
			thread_data->song->artist, 
			thread_data->song->title, 
			thread_data->song->album
	      );

	/* For the start: Enable verbosity */
	GlyOpt_verbosity(&q, 2);

	/* get metadata */
	cache = Gly_get(&q,NULL,NULL);

	/* something there? */
	if (cache != NULL)
	{
		GList *retv = NULL;
		MetaData *mtd = meta_data_new();
		mtd->type = thread_data->type; 
		mtd->plugin_name = glyros_plugin.name;
		mtd->content_type = content_type; 
		mtd->content = malloc(cache->size);
		memcpy(mtd->content, cache->data, cache->size);
		mtd->size = cache->size;	
		retv = g_list_prepend(retv,mtd);

		thread_data->callback(retv, thread_data->user_data);

		Gly_free_list(cache);
		Gly_destroy_query(&q);
		free(data);
		return NULL;
	}

	/* destroy */
	Gly_destroy_query(&q);
	free(data);

	thread_data->callback(NULL, thread_data->user_data);

	return NULL;
}

static void glyros_fetch(mpd_Song *song,MetaDataType type, 
		void (*callback)(GList *list, gpointer data),
		gpointer user_data)
{
	struct glyros_fetch_thread_data * data = g_malloc(sizeof(struct glyros_fetch_thread_data));

	data->song = song;
	data->type = type;
	data->callback = callback;
	data->user_data = user_data;

	g_thread_create(glyros_fetch_thread, (gpointer)data, FALSE, NULL);  
}



static void pref_enable_fetch(GtkWidget *con, gpointer data)
{
	MetaDataType type = GPOINTER_TO_INT(data);
	int state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(con));
	switch(type) {
		case META_ARTIST_ART:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_ARTIST_ART,state); 
			break;
		case META_ALBUM_ART:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_COVER_NAME,state); 
			break;
		case META_ARTIST_SIMILAR:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_SIMILIAR_ARTIST, state); 
			break;
		case META_SONG_SIMILAR:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_SIMILIAR_SONG,state); 
			break;                                                                                 
		case META_GENRE_SIMILAR:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_SIMILIAR_GENRE, state);
			break;
		case META_ARTIST_TXT:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_ARTIST_TXT,state); 
			break;                                                  
		case META_SONG_TXT:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_SONG_TXT,state); 
			break;
		case META_ALBUM_TXT:
			cfg_set_single_value_as_int(config, LOG_SUBCLASS, LOG_ALBUM_TXT,state);
			break;
		default:
			break;
	}
}

static void pref_add_checkbox(const char * text, MetaDataType type, const char * log_to, GtkWidget * vbox) 
{
	GtkWidget * toggleb = gtk_check_button_new_with_label(text);    
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggleb),
	cfg_get_single_value_as_int_with_default(config, LOG_SUBCLASS, log_to, TRUE)); 
	gtk_box_pack_start(GTK_BOX(vbox), toggleb, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(toggleb), "toggled", G_CALLBACK(pref_enable_fetch), GINT_TO_POINTER(type));
}

static void pref_construct(GtkWidget * con)
{
	GtkWidget * frame = gtk_frame_new("");
	gtk_label_set_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), "<b>Fetch</b>");
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	GtkWidget * vbox = gtk_vbox_new(FALSE,6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	pref_add_checkbox("Artist Images",META_ARTIST_ART,LOG_ARTIST_ART,vbox);
	pref_add_checkbox("Artist Biography",META_ARTIST_TXT,LOG_ARTIST_TXT,vbox);
	pref_add_checkbox("Similiar artist",META_ARTIST_SIMILAR,LOG_SIMILIAR_ARTIST,vbox);
	pref_add_checkbox("Album cover",META_ALBUM_ART,LOG_COVER_NAME,vbox);
	pref_add_checkbox("Songlyrics",META_SONG_TXT,LOG_SONG_TXT,vbox);
	pref_add_checkbox("Album information",META_ALBUM_TXT,LOG_ALBUM_TXT,vbox);
/*
	// Missing support for:
	pref_add_checkbox("Similiar songs",META_SONG_SIMILAR,LOG_SIMILIAR_SONG,vbox);
	pref_add_checkbox("Similiar genre",META_GENRE_SIMILAR,LOG_SIMILIAR_GENRE,vbox);
*/

	if(!glyros_get_enabled()) {
		gtk_widget_set_sensitive(GTK_WIDGET(vbox), FALSE);
	}

	gtk_widget_show_all(frame);
	gtk_container_add(GTK_CONTAINER(con), frame);
} 

static void pref_destroy(GtkWidget *con)
{
	GtkWidget *child = gtk_bin_get_child(GTK_BIN(con));
	if(child) {
		gtk_container_remove(GTK_CONTAINER(con), child);
	}
}

static gmpcMetaDataPlugin glyros_metadata_object =
{
	.get_priority   = glyros_fetch_cover_priority,
	.set_priority   = glyros_fetch_cover_priority_set,
	.get_metadata   = glyros_fetch
};

static gmpcPrefPlugin glyros_pref_object = 
{
	.construct = pref_construct,
	.destroy   = pref_destroy,
	.padding1  = NULL,
	.padding2  = NULL,
	.padding3  = NULL
};

gmpcPlugin plugin =
{
	.name           = ("Glyros Allmetadata fetcher"),
	.version        = {0,21,1},
	.plugin_type    = GMPC_PLUGIN_META_DATA,
	.init           = glyros_init,
	.pref 		= &glyros_pref_object,
	.metadata       = &glyros_metadata_object,
	.get_enabled    = glyros_get_enabled,
	.set_enabled    = glyros_set_enabled,
};
