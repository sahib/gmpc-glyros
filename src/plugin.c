/* gmpc-last.fm (GMPC plugin)
 * Copyright (C) 2006-2009 Qball Cow <qball@sarine.nl>
 * Project homepage: http://gmpcwiki.sarine.nl/
 
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

#include <config.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <gmpc/plugin.h>
#include <gmpc/gmpc_easy_download.h>
#include <gmpc/metadata.h>

#define LASTFM_API_KEY "ec1cdd08d574e93fa6ef9ad861ae795a" 
#define LASTFM_API_ROOT "http://ws.audioscrobbler.com/2.0/"
gmpcPlugin plugin;

typedef struct Query {
	MetaDataType type;
	void (*callback)(GList *list, gpointer data);
	gpointer user_data;
}Query;

static int lastfm_get_enabled()
{
	return cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "enable", TRUE);
}
static void lastfm_set_enabled(int enabled)
{
	cfg_set_single_value_as_int(config, "cover-lastfm", "enable", enabled);
}

static int lastfm_fetch_cover_priority(void){
	return cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "priority", 80);
}
static void lastfm_fetch_cover_priority_set(int priority){
	cfg_set_single_value_as_int(config, "cover-lastfm", "priority", priority);
}

static xmlNodePtr get_first_node_by_name(xmlNodePtr xml, gchar *name) {
    if(name == NULL) return NULL;
	if(xml) {
		xmlNodePtr c = xml->xmlChildrenNode;
		for(;c;c=c->next) {
			if(c->name && xmlStrEqual(c->name, (xmlChar *) name))
				return c;
		}
	}
	return NULL;
}

static GList *__lastfm_art_xml_get_artist_image(const char *data, gint size, MetaDataType mtype)
{
    GList *list = NULL;
    xmlDocPtr doc;
    if(size <= 0 || data == NULL || data[0] != '<')
        return NULL;

    doc = xmlParseMemory(data,size);
    if(doc)
    {
        xmlNodePtr root = xmlDocGetRootElement(doc);
        if(root)
        {
            /* loop through all albums */
            xmlNodePtr cur = get_first_node_by_name(root,"images");
            if(cur)
            {
                xmlNodePtr cur2 = cur->xmlChildrenNode;
                for(;cur2;cur2 = cur2->next)
                {
                    if(cur2->name)
                    {
                        if (xmlStrEqual(cur2->name, (xmlChar *)"image"))
                        {
                            xmlNodePtr cur3 = cur2->xmlChildrenNode;
                            for(;cur3;cur3 = cur3->next)
                            {
                                if(xmlStrEqual(cur3->name, (xmlChar *)"sizes"))
                                {    
                                    xmlNodePtr cur4 = cur3->xmlChildrenNode;
                                    for(;cur4;cur4 = cur4->next)
                                    {
                                        if(xmlStrEqual(cur4->name, (xmlChar *)"size"))
                                        {
                                            xmlChar *temp = xmlGetProp(cur4, (xmlChar *)"name");
                                            if(temp)
                                            {
                                                /**
                                                 * We want large image, but if that is not available, get the medium one 
                                                 */
                                                if(xmlStrEqual(temp, (xmlChar *)"original"))
                                                {
                                                    xmlChar *xurl = xmlNodeGetContent(cur4);
                                                    if(xurl){
                                                        if(strstr((char *)xurl, "noartist") == NULL){
                                                            MetaData *mtd = meta_data_new();
                                                            mtd->type = mtype;
                                                            mtd->plugin_name = plugin.name;
                                                            mtd->content_type = META_DATA_CONTENT_URI;
                                                            mtd->content  = g_strdup((char *)xurl); mtd->size = 0;
                                                            list =g_list_prepend(list, mtd);
                                                        }
                                                        xmlFree(xurl);
                                                    }
                                                }/*else if(xmlStrEqual(temp, (xmlChar *)"large") || xmlStrEqual(temp, (xmlChar *)"extralarge"))
                                                {
                                                    xmlChar *xurl = xmlNodeGetContent(cur2);
                                                    if(xurl)
                                                    {
                                                        if(strstr((char *)xurl, "noartist") == NULL){
                                                            MetaData *mtd = meta_data_new();
                                                            mtd->type = mtype;
                                                            mtd->plugin_name = plugin.name;
                                                            mtd->content_type = META_DATA_CONTENT_URI;
                                                            mtd->content  = g_strdup((char *)xurl); mtd->size = 0;
                                                            list =g_list_prepend(list, mtd);
                                                        }
                                                        xmlFree(xurl);
                                                    }
                                                }*/
                                                xmlFree(temp);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        xmlFreeDoc(doc);
    }
	return g_list_reverse(list);
}
static GList* __lastfm_art_xml_get_image(const char* data, gint size, char* type, MetaDataType mtype)
{
	GList *list = NULL;
    xmlDocPtr doc;
	if(size <= 0 || data == NULL || data[0] != '<')
		return NULL;

	doc = xmlParseMemory(data,size);
	if(doc)
	{
		xmlNodePtr root = xmlDocGetRootElement(doc);
        if(root)
        {
            /* loop through all albums */
            xmlNodePtr cur = get_first_node_by_name(root,type);
            if(cur)
            {
                xmlNodePtr cur2 = cur->xmlChildrenNode;
                for(;cur2;cur2 = cur2->next)
                {
                    if(cur2->name)
                    {
                        if (xmlStrEqual(cur2->name, (xmlChar *)"image"))
                        {

                            xmlChar *temp = xmlGetProp(cur2, (xmlChar *)"size");
                            if(temp)
                            {
                                /**
                                 * We want large image, but if that is not available, get the medium one 
                                 */
                                if(xmlStrEqual(temp, (xmlChar *)"medium"))
                                {
                                    xmlChar *xurl = xmlNodeGetContent(cur2);
                                    if(xurl){
                                        if(strstr((char *)xurl, "noartist") == NULL){
                                            MetaData *mtd = meta_data_new();
                                            mtd->type = mtype;
                                            mtd->plugin_name = plugin.name;
                                            mtd->content_type = META_DATA_CONTENT_URI;
                                            mtd->content  = g_strdup((char *)xurl); mtd->size = 0;
                                            list =g_list_append(list, mtd);
                                        }
                                        xmlFree(xurl);
                                    }
                                }else if(xmlStrEqual(temp, (xmlChar *)"large") || xmlStrEqual(temp, (xmlChar *)"extralarge"))
                                {
                                    xmlChar *xurl = xmlNodeGetContent(cur2);
                                    if(xurl)
                                    {
                                        if(strstr((char *)xurl, "noartist") == NULL){
                                            MetaData *mtd = meta_data_new();
                                            mtd->type = mtype;
                                            mtd->plugin_name = plugin.name;
                                            mtd->content_type = META_DATA_CONTENT_URI;
                                            mtd->content  = g_strdup((char *)xurl); mtd->size = 0;
                                            list =g_list_prepend(list, mtd);
                                        }
                                        xmlFree(xurl);
                                    }
                                }
                                xmlFree(temp);
                            }
                        }
                    }
                }
            }
        }
		xmlFreeDoc(doc);
	}
	return list;
}

/* get similar genres */
static MetaData* __lastfm_art_xml_get_genre_similar(const gchar* l_data, gint l_size)
{
	if(l_size <= 0 || l_data == NULL || l_data[0] != '<')
		return NULL;

	MetaData* mtd = NULL;
	xmlDocPtr doc = xmlParseMemory(l_data, l_size);
	if(doc != NULL)
	{
		xmlNodePtr root = xmlDocGetRootElement(doc);
		xmlNodePtr cur = get_first_node_by_name(root, "similartags");
		if(cur != NULL)
		{
			xmlNodePtr cur2 = cur->xmlChildrenNode;
			for(; cur2 != NULL; cur2 = cur2->next)
			{
				if(xmlStrEqual(cur2->name, (xmlChar*) "tag"))
				{
					xmlNodePtr cur3 = cur2->xmlChildrenNode;
					for(; cur3 != NULL; cur3 = cur3->next)
					{
						if(xmlStrEqual(cur3->name, (xmlChar*) "name"))
						{
							xmlChar* temp = xmlNodeGetContent(cur3);
							if(temp)
							{
								if(!mtd)
								{
									mtd = meta_data_new();
									mtd->type = META_GENRE_SIMILAR;
									mtd->plugin_name = plugin.name;
									mtd->content_type = META_DATA_CONTENT_TEXT_LIST;
									mtd->size = 0;
								}
								mtd->size++;
								mtd->content = g_list_prepend((GList*) mtd->content, g_strdup((char *)temp));
                                xmlFree(temp);
                                break;
                            }
						}
					}
				}
			}
			if(mtd != NULL)
				mtd->content = g_list_reverse((GList*) mtd->content); /* to have the match-order */
		}
		xmlFreeDoc(doc);
	}

	return mtd;
}

/*
 * Get 20 artists
 */
static MetaData* __lastfm_art_xml_get_artist_similar(const gchar* data, gint size)
{
	if(size <= 0 || data == NULL || data[0] != '<')
		return NULL;

	MetaData *mtd = NULL;
	xmlDocPtr doc = xmlParseMemory(data,size);
	if(doc)
	{
		xmlNodePtr root = xmlDocGetRootElement(doc);
		xmlNodePtr cur = get_first_node_by_name(root, "similarartists");
        if(cur)
        {
            xmlNodePtr cur2 = cur->xmlChildrenNode;
            for(;cur2;cur2=cur2->next)
            {
                if(xmlStrEqual(cur2->name, (xmlChar *)"artist"))
                {
                    xmlNodePtr cur3 = cur2->xmlChildrenNode;
                    for(;cur3;cur3=cur3->next)
                    {
                        if(xmlStrEqual(cur3->name, (xmlChar *)"name"))
                        {
                            xmlChar *temp = xmlNodeGetContent(cur3);
                            if(temp)
                            {
                                if(!mtd) {
                                    mtd = meta_data_new();
                                    mtd->type = META_ARTIST_SIMILAR;
                                    mtd->plugin_name = plugin.name;
                                    mtd->content_type = META_DATA_CONTENT_TEXT_LIST;
                                    mtd->size = 0;
                                }
                                mtd->size++;
								mtd->content = g_list_prepend((GList*) mtd->content, g_strdup((char *)temp));
                                xmlFree(temp);
                            }
                        }
                    }
                }
            }
			if(mtd != NULL)
				mtd->content = g_list_reverse((GList*) mtd->content);
        }
		xmlFreeDoc(doc);
		
	}
	return mtd;
}

/*
 * Get 20Songs 
 */
static MetaData* __lastfm_art_xml_get_song_similar(const gchar* data, gint size)
{
	if(size <= 0 || data == NULL || data[0] != '<')
		return NULL;

	MetaData *mtd = NULL;
	xmlDocPtr doc = xmlParseMemory(data,size);
	if(doc)
	{
		xmlNodePtr root = xmlDocGetRootElement(doc);
		xmlNodePtr cur = get_first_node_by_name(root, "similartracks");
        if(cur)
        {
            xmlNodePtr cur2 = cur->xmlChildrenNode;
            for(;cur2;cur2=cur2->next)
            {
                if(xmlStrEqual(cur2->name, (xmlChar *)"track"))
                {
                    xmlNodePtr cur3 = cur2->xmlChildrenNode;
                    xmlChar *artist = NULL;
                    xmlChar *title = NULL;
                    for(;cur3;cur3=cur3->next)
                    {
                        if(xmlStrEqual(cur3->name, (xmlChar *)"name"))
                        {
                            xmlChar *temp = xmlNodeGetContent(cur3);
                            title = temp; 
                        }
                        else if (xmlStrEqual(cur3->name, (xmlChar *)"artist")) 
                        {
                            xmlNodePtr cur4 = get_first_node_by_name(cur3, "name");
                            if(cur4){
                                xmlChar *temp = xmlNodeGetContent(cur4);
                                artist = temp; 
                            }
                        }
                    }
                    if(artist && title) {
                        if(!mtd) {
                            mtd = meta_data_new();
                            mtd->type = META_SONG_SIMILAR;
                            mtd->plugin_name = plugin.name;
							mtd->content_type = META_DATA_CONTENT_TEXT_LIST;
                            mtd->size = 0;
                        }
                        mtd->size++;
						mtd->content = g_list_prepend((GList*) mtd->content, g_strdup_printf("%s::%s", artist, title));
                    }
                    if(artist) xmlFree(artist);
                    if(title) xmlFree(title);
                }
            }
			if(mtd != NULL)
				mtd->content = g_list_reverse((GList*) mtd->content);
        }
		xmlFreeDoc(doc);
		
	}
	return mtd;
}


/** 
 * Get album image
 */
/**
 * Get artist info 
 */


static gchar* __lastfm_art_xml_get_artist_bio(const gchar* data , gint size)
{
	xmlDocPtr doc = xmlParseMemory(data,size);
	gchar* info=NULL;
	if(doc)
	{
		xmlNodePtr root = xmlDocGetRootElement(doc);
		xmlNodePtr bio = get_first_node_by_name(get_first_node_by_name(get_first_node_by_name(root,"artist"),"bio"),"content");
		if(bio)
		{
			xmlChar *temp = xmlNodeGetContent(bio);
			info = g_strdup((gchar*) temp);
			xmlFree(temp);
		}
	}
	xmlFreeDoc(doc);

	return info;
}


/**
 * Preferences 
 */
static void pref_destroy(GtkWidget *con)
{
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(con));
    if(child) {
        gtk_container_remove(GTK_CONTAINER(con), child);
    }
}

static void pref_enable_fetch(GtkWidget *con, gpointer data)
{
    MetaDataType type = GPOINTER_TO_INT(data);
    int state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(con));
    switch(type) {
        case META_ARTIST_ART:
            cfg_set_single_value_as_int(config, "cover-lastfm", "fetch-art-artist",state); 
            break;
        case META_ALBUM_ART:
            cfg_set_single_value_as_int(config, "cover-lastfm", "fetch-art-album",state); 
            break;
        case META_ARTIST_SIMILAR:
            cfg_set_single_value_as_int(config, "cover-lastfm", "fetch-similar-artist",state); 
            break;
        case META_SONG_SIMILAR:
            cfg_set_single_value_as_int(config, "cover-lastfm", "fetch-similar-song",state); 
            break;                                                                                 
		case META_GENRE_SIMILAR:
			cfg_set_single_value_as_int(config, "cover-lastfm", "fetch-similar-genre", state);
			break;
        case META_ARTIST_TXT:
            cfg_set_single_value_as_int(config, "cover-lastfm", "fetch-biography-artist",state); 
            break;                                                                                 
        default:
            break;
    }
}


static void pref_construct(GtkWidget *con)
{
    GtkWidget *frame,*vbox;
    GtkWidget *a_a_ck, *a_b_ck, *a_s_ck,*c_a_ck, *s_s_ck, *s_g_ck;

    /**
     * Enable/Disable checkbox
     */
    frame = gtk_frame_new("");
    gtk_label_set_markup(GTK_LABEL(gtk_frame_get_label_widget(GTK_FRAME(frame))), "<b>Fetch</b>");
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    vbox = gtk_vbox_new(FALSE,6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(frame), vbox);

    /* Fetch artist art */
    a_a_ck = gtk_check_button_new_with_label(_("Artist images"));    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a_a_ck),
        cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-art-artist", TRUE)); 
    gtk_box_pack_start(GTK_BOX(vbox), a_a_ck, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(a_a_ck), "toggled", G_CALLBACK(pref_enable_fetch), GINT_TO_POINTER(META_ARTIST_ART));

    /* Fetch artist text*/
    a_b_ck = gtk_check_button_new_with_label(_("Artist biography"));    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a_b_ck),
        cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-biography-artist", TRUE)); 
    gtk_box_pack_start(GTK_BOX(vbox), a_b_ck, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(a_b_ck), "toggled", G_CALLBACK(pref_enable_fetch), GINT_TO_POINTER(META_ARTIST_TXT));

    /* Fetch similar artists */
    a_s_ck = gtk_check_button_new_with_label(_("Similar artists"));    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(a_s_ck),
        cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-similar-artist", TRUE)); 
    gtk_box_pack_start(GTK_BOX(vbox), a_s_ck, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(a_s_ck), "toggled", G_CALLBACK(pref_enable_fetch), GINT_TO_POINTER(META_ARTIST_SIMILAR));

    /* Fetch artist art */
    c_a_ck = gtk_check_button_new_with_label(_("Album cover"));    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(c_a_ck),
        cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-art-album", TRUE)); 
    gtk_box_pack_start(GTK_BOX(vbox), c_a_ck, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(c_a_ck), "toggled", G_CALLBACK(pref_enable_fetch), GINT_TO_POINTER(META_ALBUM_ART));

    /* Fetch similar songs */
    s_s_ck = gtk_check_button_new_with_label(_("Similar songs"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s_s_ck),
        cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-similar-song", TRUE)); 
    gtk_box_pack_start(GTK_BOX(vbox), s_s_ck, FALSE, TRUE, 0);
    g_signal_connect(G_OBJECT(s_s_ck), "toggled", G_CALLBACK(pref_enable_fetch), GINT_TO_POINTER(META_SONG_SIMILAR));

	/* Fetch similar genre */
	s_g_ck = gtk_check_button_new_with_label(_("Similar genres"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(s_g_ck),
		cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-similar-genre", TRUE));
	gtk_box_pack_start(GTK_BOX(vbox), s_g_ck, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(s_g_ck), "toggled", G_CALLBACK(pref_enable_fetch), GINT_TO_POINTER(META_GENRE_SIMILAR));


    if(!lastfm_get_enabled()) {
        gtk_widget_set_sensitive(GTK_WIDGET(vbox), FALSE);
    }

    gtk_widget_show_all(frame);
    gtk_container_add(GTK_CONTAINER(con), frame);
}
/**
 * Similarsong  
 */
static void similar_song_callback(const GEADAsyncHandler *handle, GEADStatus status, gpointer user_data)
{
	Query *q = (Query *)user_data;
	GList *list = NULL;
	if(status == GEAD_PROGRESS) return;
	if(status == GEAD_DONE)
	{
		goffset size=0;
		const char *data = gmpc_easy_handler_get_data(handle, &size);
		MetaData *mtd = __lastfm_art_xml_get_song_similar((char *)data,(gint)size);
        if(mtd) {
            list = g_list_append(list, mtd);
        }
    }
	q->callback(list, q->user_data);
	g_slice_free(Query, q);
}
/**
 * Similar artist
 */
static void similar_artist_callback(const GEADAsyncHandler *handle, GEADStatus status, gpointer user_data)
{
	Query *q = (Query *)user_data;
	GList *list = NULL;
	if(status == GEAD_PROGRESS) return;
	if(status == GEAD_DONE)
	{
		goffset size=0;
		const gchar* data = gmpc_easy_handler_get_data(handle, &size);
		MetaData *mtd = __lastfm_art_xml_get_artist_similar(data, size);
        if(mtd){
            list = g_list_append(list, mtd);
        }
    }
	q->callback(list, q->user_data);
	g_slice_free(Query, q);
}
/**
 * Similar genre
 */
static void similar_genre_callback(const GEADAsyncHandler *handle, GEADStatus status, gpointer user_data)
{
	if(status == GEAD_PROGRESS)
		return;

	Query* q = (Query*) user_data;
	GList* list = NULL;
	if(status == GEAD_DONE)
	{
		goffset size = 0;
		const gchar* data = gmpc_easy_handler_get_data(handle, &size);
		MetaData* mtd = __lastfm_art_xml_get_genre_similar(data, size);
		if(mtd)
			list = g_list_append(list, mtd);
	}
	q->callback(list, q->user_data);
	g_slice_free(Query, q);
}
/****
 * Get biograpy  new style
 */

static void biography_callback(const GEADAsyncHandler *handle, GEADStatus status, gpointer user_data)
{
	Query *q = (Query *)user_data;
	GList *list = NULL;
	if(status == GEAD_PROGRESS) return;
	if(status == GEAD_DONE)
	{
		goffset size=0;
		const gchar *data = gmpc_easy_handler_get_data(handle, &size);
		char* url = __lastfm_art_xml_get_artist_bio(data, size);
		/* strip html */
		if(url)
		{
			int i=0;
			int j=0,depth=0;;
			for(j=0; j < strlen(url);j++)
			{
				if(url[j] == '<') depth++;
				else if(url[j] == '>' && depth) depth--;
				else if(depth == 0)
				{
					/* Quick and dirty html unescape*/
					if(strncasecmp(&url[j], "&lt;", 4) == 0){
						url[i] = url[j];	
						i++;
						j+=3;
					}else if (strncasecmp(&url[j], "&gt;", 4) == 0){
						url[i] = url[j];	
						i++;
						j+=3;
					}else if (strncasecmp(&url[j], "&quot;", 6) == 0){
						url[i] = url[j];	
						i++;
						j+=5;
					}else if (strncasecmp(&url[j], "&amp;", 5) == 0){
						url[i] = url[j];	
						i++;
						j+=4;
					}
					else{
						url[i] = url[j];	
						i++;
					}
				}
			}
			url[i] = '\0';
            if(i > 0){
                MetaData *mtd = meta_data_new();
                mtd->type = META_ARTIST_TXT;
                mtd->plugin_name = plugin.name;
                mtd->content_type = META_DATA_CONTENT_TEXT;
                mtd->content = url;
                mtd->size = i;
                list = g_list_append(list, mtd);
            }
            else g_free(url);
		}

	}
	q->callback(list, q->user_data);
	g_slice_free(Query, q);
}
/****
 * Get album images new style
 */

static void album_image_callback(const GEADAsyncHandler *handle, GEADStatus status, gpointer user_data)
{
	Query *q = (Query *)user_data;
	GList *list = NULL;
	if(status == GEAD_PROGRESS) return;
	if(status == GEAD_DONE)
	{
		goffset size=0;
		const gchar* data = gmpc_easy_handler_get_data(handle, &size);
		list = __lastfm_art_xml_get_image(data, size, "album", META_ALBUM_ART);
	}
	q->callback(list, q->user_data);
	g_slice_free(Query, q);
}
/**
 * Get artist image new style
 */
static void artist_image_callback(const GEADAsyncHandler *handle, GEADStatus status, gpointer user_data)
{
	Query *q = (Query *)user_data;
	GList *list = NULL;
	if(status == GEAD_PROGRESS) return;
	if(status == GEAD_DONE)
	{
		goffset size=0;
		const gchar* data = gmpc_easy_handler_get_data(handle, &size);
		list = __lastfm_art_xml_get_artist_image(data, size, META_ARTIST_ART);

	}
	q->callback(list, q->user_data);
	g_slice_free(Query, q);
}

static void lastfm_fetch_get_uris(mpd_Song *song, MetaDataType type, void (*callback)(GList *list, gpointer data), gpointer user_data)
{
	g_debug("Query last.fm api v2");
    if(song->artist != NULL && type == META_ARTIST_ART && 
			cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-art-artist", TRUE))
	{
		char furl[1024];
		gchar *artist = gmpc_easy_download_uri_escape(song->artist);
		Query *q = g_slice_new0(Query);

		q->callback = callback;
		q->user_data = user_data;
		snprintf(furl,1024,LASTFM_API_ROOT"?method=artist.getImages&artist=%s&api_key=%s", artist,LASTFM_API_KEY);
		g_debug("url: '%s'", furl);
		gmpc_easy_async_downloader(furl, artist_image_callback, q); 
		g_free(artist);
		return;
	}
	else if (song->artist != NULL && song->album != NULL &&  type == META_ALBUM_ART && 
			cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-art-album", TRUE))
	{
		char furl[1024];
		gchar *artist = gmpc_easy_download_uri_escape(song->artist);
		gchar *album = gmpc_easy_download_uri_escape(song->album);
		Query *q = g_slice_new0(Query);

		q->callback = callback;
		q->user_data = user_data;
		snprintf(furl,1024,LASTFM_API_ROOT"?method=album.getinfo&artist=%s&album=%s&api_key=%s", artist,album,LASTFM_API_KEY);
		g_debug("url: '%s'", furl);
		gmpc_easy_async_downloader(furl, album_image_callback, q); 
		g_free(artist);
		g_free(album);
		return;
	}

	/* Fetch artist info */
	else if (song->artist != NULL && type == META_ARTIST_TXT && 
			cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-biography-artist", TRUE))
	{

		char furl[1024];
		gchar *artist = gmpc_easy_download_uri_escape(song->artist);
		Query *q = g_slice_new0(Query);

		q->callback = callback;
		q->user_data = user_data;
		snprintf(furl,1024, LASTFM_API_ROOT"?method=artist.getinfo&artist=%s&api_key=%s", artist,LASTFM_API_KEY);
		g_debug("url: '%s'", furl);
		gmpc_easy_async_downloader(furl, biography_callback, q); 
		g_free(artist);

		return;
	}
    else if (song->artist != NULL && type == META_ARTIST_SIMILAR
            && cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-similar-artist", TRUE))
    {
        char furl[1024];
        char *artist = gmpc_easy_download_uri_escape(song->artist);
		Query *q = g_slice_new0(Query);

		q->callback = callback;
		q->user_data = user_data;
        snprintf(furl,1024,LASTFM_API_ROOT"?method=artist.getsimilar&artist=%s&api_key=%s", artist,LASTFM_API_KEY);
		g_debug("url: '%s'", furl);
        g_free(artist);
		gmpc_easy_async_downloader(furl, similar_artist_callback, q); 
        return;
	}
	else if (song->genre != NULL && type == META_GENRE_SIMILAR
			&& cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-similar-genre", TRUE))
	{
		Query *q = g_slice_new0(Query);
		q->callback = callback;
		q->user_data = user_data;

		gchar* genre = gmpc_easy_download_uri_escape(song->genre);
		gchar* furl = g_strdup_printf(LASTFM_API_ROOT"?method=tag.getsimilar&tag=%s&api_key=%s", genre, LASTFM_API_KEY);
		g_debug("url: '%s'", furl);
		gmpc_easy_async_downloader(furl, similar_genre_callback, q);
		g_free(genre);
		g_free(furl);

		return;
    }else if (song->title != NULL && song->artist != NULL && type == META_SONG_SIMILAR && cfg_get_single_value_as_int_with_default(config, "cover-lastfm", "fetch-similar-song", TRUE))
    {

        char furl[1024];
        char *artist = gmpc_easy_download_uri_escape(song->artist);
        char *title =  gmpc_easy_download_uri_escape(song->title);
		Query *q = g_slice_new0(Query);

		q->callback = callback;
		q->user_data = user_data;
        snprintf(furl,1024,LASTFM_API_ROOT"?method=track.getsimilar&artist=%s&track=%s&api_key=%s", artist,title,LASTFM_API_KEY);
		g_debug("url: '%s'", furl);
        g_free(artist);
		gmpc_easy_async_downloader(furl, similar_song_callback, q); 
        return;
    }

	callback(NULL, user_data);
}
gmpcPrefPlugin pref = {
    .construct      = pref_construct,
    .destroy        = pref_destroy
};

/**
 * Metadata Plugin
 */
gmpcMetaDataPlugin lf_cover = {
	.get_priority   = lastfm_fetch_cover_priority,
	.set_priority   = lastfm_fetch_cover_priority_set,
	.get_metadata = lastfm_fetch_get_uris
};

int plugin_api_version = PLUGIN_API_VERSION;
static void lastfm_init(void)
{
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
}
static const gchar *lastfm_get_translation_domain(void)
{
    return GETTEXT_PACKAGE;
}
gmpcPlugin plugin = {
	.name           = N_("Last FM metadata fetcher"),
	.version        = {PLUGIN_MAJOR_VERSION,PLUGIN_MINOR_VERSION,PLUGIN_MICRO_VERSION},
	.plugin_type    = GMPC_PLUGIN_META_DATA,
    .init           = lastfm_init,
	.metadata       = &lf_cover,
    .pref           = &pref,
	.get_enabled    = lastfm_get_enabled,
	.set_enabled    = lastfm_set_enabled,
    .get_translation_domain = lastfm_get_translation_domain
};

/* vim:set ts=4 sw=4: */
