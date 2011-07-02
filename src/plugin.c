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

gmpcPlugin glyros_plugin;

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
};
static int glyros_fetch_cover_priority(void)
{
	return cfg_get_single_value_as_int_with_default(config, "cover-glyros", "priority", 100);
}
static void glyros_fetch_cover_priority_set(int priority)
{
	cfg_set_single_value_as_int(config, "cover-glyros", "priority", priority);
}
static int glyros_get_enabled(void)
{
	return cfg_get_single_value_as_int_with_default(config, "cover-glyros", "enable", TRUE);
}
static void glyros_set_enabled(int enabled)
{
	cfg_set_single_value_as_int(config, "cover-glyros", "enable", enabled);
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

	/* set default type */
	GlyOpt_type(&q, GET_UNSURE);

	/* set type */
	if (glyros_get_enabled() == TRUE) 
	{
		if (thread_data->song->artist != NULL)
		{
			if (thread_data->type == META_ARTIST_ART)
			{
				GlyOpt_type(&q, GET_ARTIST_PHOTOS);
				content_type = META_DATA_CONTENT_RAW;
			}
			else if (thread_data->type == META_ARTIST_TXT)
			{
				GlyOpt_type(&q, GET_ARTISTBIO);
				content_type = META_DATA_CONTENT_TEXT;
			}
			else if (thread_data->type == META_ARTIST_SIMILAR) 
			{
				GlyOpt_type(&q, GET_SIMILIAR_ARTISTS);
				content_type = META_DATA_CONTENT_TEXT;
			}
			else if (thread_data->type == META_ALBUM_ART && thread_data->song->album != NULL)
			{
				GlyOpt_type(&q, GET_COVERART);
				content_type = META_DATA_CONTENT_RAW;
			}
			else if (thread_data->type == META_ALBUM_TXT && thread_data->song->album != NULL)
			{
				/* not supported */
			}
			else if (thread_data->type == META_SONG_TXT && thread_data->song->title != NULL)
			{
				GlyOpt_type(&q, GET_LYRICS);
				content_type = META_DATA_CONTENT_TEXT;
			}
			else if (thread_data->type == META_SONG_SIMILAR && thread_data->song->title != NULL)
			{
				/* not supported */
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

		puts("PRE");
		thread_data->callback(retv, thread_data->user_data);

		// SEE IT!
		//g_list_free(retv);
		Gly_free_list(cache);
		Gly_destroy_query(&q);
		free(data);
		puts("AFTER");
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

	pthread_t t;
	pthread_create(&t,  NULL, glyros_fetch_thread, (void*)data);
	//	g_thread_create(glyros_fetch_thread, data, FALSE, NULL);
}

static gmpcMetaDataPlugin glyros_metadata_object =
{
	.get_priority   = glyros_fetch_cover_priority,
	.set_priority   = glyros_fetch_cover_priority_set,
	.get_metadata   = glyros_fetch
};

gmpcPlugin plugin =
{
	.name           = ("Glyros Artist and Album Image Fetcher"),
	.version        = {0,21,0},
	.plugin_type    = GMPC_PLUGIN_META_DATA,
	.init           = glyros_init,
	.metadata       = &glyros_metadata_object,
	.get_enabled    = glyros_get_enabled,
	.set_enabled    = glyros_set_enabled,
};
