/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003	Peter Alm, Tobias Rundstr�m, Anders Gustafsson
 * 
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 * 
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *                   
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */





/** @file
 *  Controls playlist
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <math.h>

#include "xmms/playlist.h"
#include "xmms/dbus.h"
#include "xmms/playlist_entry.h"
#include "xmms/util.h"
#include "xmms/signal_xmms.h"
#include "xmms/mediainfo.h"


/** @defgroup PlaylistClientMethods PlaylistClientMethods
  * @ingroup Playlist
  * @brief the playlist methods that could be used by the client
  */

/** @defgroup Playlist Playlist
  * @ingroup XMMSServer
  * @brief This is the playlist control.
  *
  * A playlist is a central thing in the XMMS server, it
  * tells us what to do after we played the following entry
  * @{
  */

/* Internal macro to emit XMMS_SIGNAL_PLAYLIST_CHANGED */
#define XMMS_PLAYLIST_CHANGED_MSG(ttype,iid,argument) do { \
	xmms_playlist_changed_msg_t *chmsg; \
	chmsg = g_new0 (xmms_playlist_changed_msg_t, 1);\
	chmsg->type = ttype; chmsg->id=iid; chmsg->arg=argument; \
	xmms_object_emit_f (XMMS_OBJECT (playlist), XMMS_SIGNAL_PLAYLIST_CHANGED, XMMS_OBJECT_METHOD_ARG_PLCH, chmsg);\
	g_free (chmsg); \
} while (0)

typedef enum {
	XMMS_PLAYLIST_MODE_NONE,
	XMMS_PLAYLIST_MODE_REPEAT_ALL
} xmms_playlist_mode_t;

/** Playlist structure */
struct xmms_playlist_St {
	xmms_object_t object;
	/** The current list first node. */
	GList *list;
	/** Next song that will be retured by xmms_playlist_get_next */
	GList *nextentry;

	xmms_playlist_mode_t mode;

	GHashTable *id_table;
	guint nextid;

	GMutex *mutex;
	GCond *cond;
	gboolean is_waiting;

	xmms_mediainfo_thread_t *mediainfothr;

};

static void xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property, xmms_error_t *err);
static void xmms_playlist_shuffle (xmms_playlist_t *playlist, xmms_error_t *err);
static void xmms_playlist_clear (xmms_playlist_t *playlist, xmms_error_t *err);
static gboolean xmms_playlist_id_move (xmms_playlist_t *playlist, guint id, gint steps, xmms_error_t *err);
static void xmms_playlist_destroy (xmms_object_t *object);
static void xmms_playlist_set_next (xmms_playlist_t *playlist, guint32 type, gint32 moment, xmms_error_t *error);
static void on_playlist_mode_changed (xmms_object_t *object, gconstpointer data, gpointer udata);
static xmms_playlist_mode_t playlist_mode_from_str (const gchar *mode);

/*
 * Public functions
 */

/** Lock the playlist for changes and queries */
#define XMMS_PLAYLIST_LOCK(a) g_mutex_lock (a->mutex)
/** Unlock the playlist for changes and queries */
#define XMMS_PLAYLIST_UNLOCK(a) g_mutex_unlock (a->mutex)

/** Gererate statistics for this playlist */
GList *
xmms_playlist_stats (xmms_playlist_t *playlist, GList *list)
{
	gchar *tmp;

	XMMS_PLAYLIST_LOCK (playlist);
	tmp = g_strdup_printf ("playlist.entries=%u", xmms_playlist_entries_total (playlist));
	list = g_list_append (list, tmp);
	XMMS_PLAYLIST_UNLOCK (playlist);

	return list;

}


/** Total number of entries in the playlist. */

guint
xmms_playlist_entries_total (xmms_playlist_t *playlist)
{
	guint ret;
	ret = g_hash_table_size (playlist->id_table);
	return ret;
}

/** Number of entries *after* the nextsong pointer. */

guint
xmms_playlist_entries_left (xmms_playlist_t *playlist)
{
	guint ret;
	ret = g_list_length (playlist->nextentry);
	return ret;
}

/** Shuffles the playlist
  * @ingroup PlaylistClientMethods
  */
XMMS_METHOD_DEFINE (shuffle, xmms_playlist_shuffle, xmms_playlist_t *, NONE, NONE, NONE);

/** shuffles playlist */
static void
xmms_playlist_shuffle (xmms_playlist_t *playlist, xmms_error_t *err)
{
	gint len, i, j;
	GList *node, **ptrs;

	g_return_if_fail (playlist);

	XMMS_PLAYLIST_LOCK (playlist);

	len = xmms_playlist_entries_total (playlist);
	if (!len) {
		XMMS_PLAYLIST_UNLOCK (playlist);
		return;
	}

	ptrs = g_new (GList *, len);

	for (node = playlist->list, i = 0;
	     i < len && node;
	     node = g_list_next (node), i++)
		ptrs[i] = node;

	j = random() % len;
	playlist->list = ptrs[j];
	ptrs[j]->next = NULL;
	ptrs[j] = ptrs[0];

	for (i = 1; i < len; i++) {
		j = random() % (len - i);
		playlist->list->prev = ptrs[i + j];
		ptrs[i + j]->next = playlist->list;
		playlist->list = ptrs[i + j];
		ptrs[i + j] = ptrs[i];
	}

	playlist->list->prev = NULL;

	g_free (ptrs);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SHUFFLE, 0, 0);

	g_cond_signal (playlist->cond);

	XMMS_PLAYLIST_UNLOCK (playlist);
}

/** Removes the id from the playlist
  * @ingroup PlaylistClientMethods
  */

XMMS_METHOD_DEFINE (remove, xmms_playlist_id_remove, xmms_playlist_t *, NONE, UINT32, NONE);

/** removes entry from playlist */
gboolean 
xmms_playlist_id_remove (xmms_playlist_t *playlist, guint id, xmms_error_t *err)
{
	GList *node;

	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (id, FALSE);

	XMMS_PLAYLIST_LOCK (playlist);
	node = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER (id));
	if (!node) {
		XMMS_PLAYLIST_UNLOCK (playlist);
		xmms_error_set (err, XMMS_ERROR_NOENT, "Trying to remove nonexistant playlist entry");
		return FALSE;
	}
	if (node == playlist->nextentry) {
		playlist->nextentry = g_list_next (playlist->nextentry);
	}
	g_hash_table_remove (playlist->id_table, GUINT_TO_POINTER (id));
	xmms_object_unref (node->data);
	playlist->list = g_list_remove_link (playlist->list, node);
	g_list_free_1 (node);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_REMOVE, id, 0);

	g_cond_signal (playlist->cond);
	
	XMMS_PLAYLIST_UNLOCK (playlist);

	return TRUE;
}

/** Move the id a certain number of steps.
 * @ingroup PlaylistClientMethods
 */
XMMS_METHOD_DEFINE (move, xmms_playlist_id_move, xmms_playlist_t *, NONE, INT32, INT32);

/** move entry in playlist */
static gboolean
xmms_playlist_id_move (xmms_playlist_t *playlist, guint id, gint steps, xmms_error_t *err)
{
	xmms_playlist_entry_t *entry;
	GList *node;
	gint i;

	g_return_val_if_fail (playlist, FALSE);
	g_return_val_if_fail (id, FALSE);
	g_return_val_if_fail (steps != 0, FALSE);

	XMMS_DBG ("Moving %d, %d steps", id, steps);

	XMMS_PLAYLIST_LOCK (playlist);
	
	node = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER (id));
	
	if (!node) {
		XMMS_PLAYLIST_UNLOCK (playlist);
		xmms_error_set (err, XMMS_ERROR_NOENT, "Trying to move nonexistant playlist entry");
		return FALSE;
	}

	entry = node->data;

	if (steps > 0) {
		GList *next=node->next;

		playlist->list = g_list_remove_link (playlist->list, node);

		for (i=0; next && (i < steps); i++) {
			next = g_list_next (next);
		}

		if (next) {
			playlist->list = g_list_insert_before (playlist->list, next, node->data);
		} else {
			playlist->list = g_list_append (playlist->list, node->data);
		}

		g_list_free_1 (node);
	} else {
		GList *prev=node->prev;

		playlist->list = g_list_remove_link (playlist->list, node);

		for (i=0; prev && (i < abs (steps) - 1); i++) {
			prev = g_list_previous (prev);
		}

		if (prev) {
			playlist->list= g_list_insert_before (playlist->list, prev, node->data);
		} else {
			playlist->list = g_list_prepend (playlist->list, node->data);
		}
		g_list_free_1 (node);

	}

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_MOVE, id, steps);

	g_cond_signal (playlist->cond);

	XMMS_PLAYLIST_UNLOCK (playlist);

	return TRUE;

}

/**
  * Convenient function for adding a URL to the playlist,
  * Creates a #xmms_playlist_entry_t for you and adds it
  * to the list.
  *
  * @param playlist the playlist to add it URL to.
  * @param nurl the URL to add
  * @param err an #xmms_error_t that should be defined upon error.
  * @return TRUE on success and FALSE otherwise.
  */
gboolean
xmms_playlist_addurl (xmms_playlist_t *playlist, gchar *nurl, xmms_error_t *err)
{
	gboolean res;

	xmms_playlist_entry_t *entry = xmms_playlist_entry_new (nurl);
	if (!entry) {
		xmms_error_set (err, XMMS_ERROR_OOM, "Could not allocate memory for entry");
		return FALSE;
	}

	res = xmms_playlist_add (playlist, entry, XMMS_PLAYLIST_APPEND);

	/* Since playlist_add will reference this entry now we turn
	   total control to it */
	xmms_object_unref (entry);

	return res;
}

/** Add a URL to the playlist
 * @ingroup PlaylistClientMethods
 */
XMMS_METHOD_DEFINE (add, xmms_playlist_addurl, xmms_playlist_t *, NONE, STRING, NONE);

/**
 * Add entries from medialib
 *
 * @return TRUE on success and FALSE otherwise.
 */
gboolean
xmms_playlist_medialibadd (xmms_playlist_t *playlist, gchar *query, xmms_error_t *err)
{
	GList *res, *n;

	res = xmms_medialib_select_entries (query, err);

	if (xmms_error_iserror(err)) {
		return FALSE;
	}

	for (n = res; n; n = g_list_next (n)) {
		/** @todo
		    this could be better,
		    setting ids and using g_list_concat and stuff */
		xmms_playlist_add (playlist, n->data, XMMS_PLAYLIST_APPEND);
		xmms_object_unref (n->data);
	}
	g_list_free (res);

	return TRUE;
}

/** Add entries from medialib to the playlist
 * @ingroup PlaylistClientMethods
 */
XMMS_METHOD_DEFINE (medialibadd, xmms_playlist_medialibadd, xmms_playlist_t *, NONE, STRING, NONE);


/** Adds a xmms_playlist_entry_t to the playlist.
 *
 *  This will append or prepend the entry according to
 *  the option.
 *  This function will wake xmms_playlist_wait.
 *  @param playlist the playlist to add the entry to.
 *  @param options should be XMMS_PLAYLIST_APPEND or XMMS_PLAYLIST_PREPEND
 *  @param file the #xmms_playlist_entry_t to add
 */

gboolean
xmms_playlist_add (xmms_playlist_t *playlist, xmms_playlist_entry_t *file, gint options)
{
	GList *node;

	g_return_val_if_fail (file, FALSE);

	XMMS_PLAYLIST_LOCK (playlist);

	xmms_playlist_entry_id_set (file, playlist->nextid++);
	xmms_object_ref (file); /* reference this entry */
	
	switch (options) {
		case XMMS_PLAYLIST_APPEND:
			playlist->list = g_list_append (playlist->list, file);
			node = g_list_last (playlist->list);
			break;
		case XMMS_PLAYLIST_PREPEND:
			playlist->list = g_list_prepend (playlist->list, file);
			node = playlist->list;
			break;
	}
	
	g_hash_table_insert (playlist->id_table, 
			     GUINT_TO_POINTER (xmms_playlist_entry_id_get (file)), 
			     node);

	if (!playlist->nextentry)
		playlist->nextentry = node;
	
	XMMS_PLAYLIST_UNLOCK (playlist);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_ADD, 
				   xmms_playlist_entry_id_get (file),
				   options);

	g_cond_signal (playlist->cond);

	return TRUE;

}

/** Return the mediainfo for the entry
 * @ingroup PlaylistClientMethods
 */

XMMS_METHOD_DEFINE (getmediainfo, xmms_playlist_get_byid, xmms_playlist_t *, PLAYLIST_ENTRY, UINT32, NONE);

/** Get a entry based on position in playlist.
 * 
 *  @returns a xmms_playlist_entry_t that lives on the given position.
 *  @param pos is the position in the playlist where 0 is the first.
 */
xmms_playlist_entry_t *
xmms_playlist_get_byid (xmms_playlist_t *playlist, guint id, xmms_error_t *err)
{
	GList *r = NULL;

	g_return_val_if_fail (playlist, NULL);

	XMMS_PLAYLIST_LOCK (playlist);

	r = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER(id));

	if (!r) {
		xmms_error_set (err, XMMS_ERROR_NOENT, "Trying to query nonexistant playlist entry");
		XMMS_PLAYLIST_UNLOCK (playlist);
		return NULL;
	}

	xmms_object_ref ((xmms_playlist_entry_t *)r->data);

	XMMS_PLAYLIST_UNLOCK (playlist);

	return r->data;

}

/** Clear the playlist
 * @ingroup PlaylistClientMethods
 */
XMMS_METHOD_DEFINE (clear, xmms_playlist_clear, xmms_playlist_t *, NONE, NONE, NONE);

/** Clear the playlist */
static void
xmms_playlist_clear (xmms_playlist_t *playlist, xmms_error_t *err)
{
	GList *node;

	g_return_if_fail (playlist);

	XMMS_PLAYLIST_LOCK (playlist);

	for (node = playlist->list; node; node = g_list_next (node)) {
		xmms_playlist_entry_t *entry = node->data;
		xmms_object_unref (entry);
	}

	g_list_free (playlist->list);
	playlist->list = NULL;
	playlist->nextentry = NULL;
	g_hash_table_destroy (playlist->id_table);
	playlist->id_table = g_hash_table_new (g_direct_hash, g_direct_equal);

	XMMS_PLAYLIST_UNLOCK (playlist);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_CLEAR, 0, 0);

}

/** Get next entry.
 *
 */

xmms_playlist_entry_t *
xmms_playlist_get_next_entry (xmms_playlist_t *playlist)
{
	xmms_playlist_entry_t *r = NULL;

	g_return_val_if_fail (playlist, NULL);

	XMMS_PLAYLIST_LOCK (playlist);

	if (playlist->nextentry) {
		r = playlist->nextentry->data;
	}

	if (!r) {
		/* next entry is the first song again, but only return
		 * that entry, if the user wants us to
		 */
		playlist->nextentry = playlist->list;

		if (playlist->mode == XMMS_PLAYLIST_MODE_REPEAT_ALL) {
			r = playlist->nextentry->data;
		}
	}

	if (r) {
		xmms_object_ref (r);
		playlist->nextentry = g_list_next (playlist->nextentry);
	}

	XMMS_PLAYLIST_UNLOCK (playlist);
	
	return r;

}


/** Set the nextentry pointer in the playlist.
 *
 *  This will set the pointer for the next entry to be
 *  returned by xmms_playlist_get_next. This function
 *  will also wake xmms_playlist_wait
 */

gboolean
xmms_playlist_set_current_position (xmms_playlist_t *playlist, guint id)
{
	GList *entry;
	g_return_val_if_fail (playlist, FALSE);

	entry = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER(id));
	if (!entry)
		return FALSE;

	playlist->nextentry = entry;

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SET_POS, id, 0);

	g_cond_signal (playlist->cond);

	return TRUE;
}


static gint
xmms_playlist_entry_compare (gconstpointer a, gconstpointer b, gpointer data)
{
	gchar *prop = data;
	const xmms_playlist_entry_t *entry1 = a;
	const xmms_playlist_entry_t *entry2 = b;
	const gchar *tmpa=NULL, *tmpb=NULL;

	tmpa = xmms_playlist_entry_property_get (entry1, prop);
	tmpb = xmms_playlist_entry_property_get (entry2, prop);

	return g_strcasecmp (tmpa, tmpb);
}

/** Sort the playlist 
 * @ingroup PlaylistClientMethods
 */
XMMS_METHOD_DEFINE (sort, xmms_playlist_sort, xmms_playlist_t *, NONE, STRING, NONE);

/** Sorts the playlist by properties.
 *
 *  This will sort the list.
 *  @param property tells xmms_playlist_sort which property it
 *  should use when sorting. 
 */

static void
xmms_playlist_sort (xmms_playlist_t *playlist, gchar *property, xmms_error_t *err)
{

	g_return_if_fail (playlist);
	XMMS_DBG ("Sorting on %s", property);

	XMMS_PLAYLIST_LOCK (playlist);

	playlist->list = g_list_sort_with_data (playlist->list, xmms_playlist_entry_compare, property);

	XMMS_PLAYLIST_UNLOCK (playlist);

	XMMS_PLAYLIST_CHANGED_MSG (XMMS_PLAYLIST_CHANGED_SORT, 0, 0);

}

XMMS_METHOD_DEFINE (set_next, xmms_playlist_set_next, xmms_playlist_t *, NONE, UINT32, INT32);

static void
xmms_playlist_set_next (xmms_playlist_t *playlist, guint32 type, gint32 moment, xmms_error_t *error)
{
	GList *next;

	g_return_if_fail (playlist);
	
	XMMS_PLAYLIST_LOCK (playlist);

	next = playlist->nextentry;

	if (type == XMMS_PLAYLIST_SET_NEXT_RELATIVE) {
		while (moment && next) {
			if (moment > 0) {
				next = g_list_next (next);
				moment--;
			} else {
				next = g_list_previous (next);
				moment++;
			}
		}
	} else if (type == XMMS_PLAYLIST_SET_NEXT_BYID) {
		next = g_hash_table_lookup (playlist->id_table, GUINT_TO_POINTER (moment));
	}

	if (!next) {
		xmms_error_set (error, XMMS_ERROR_NOENT, "Trying to move to non exsisting entry");
	} else {
		playlist->nextentry = next;
	}

	XMMS_PLAYLIST_UNLOCK (playlist);
}

/** List the playlist
 * @ingroup PlaylistClientMethods
 */
XMMS_METHOD_DEFINE (list, xmms_playlist_list, xmms_playlist_t *, UINTLIST, NONE, NONE);

/** Lists the current playlist.
 *
 * @returns A newly allocated GList with the current playlist.
 * Remeber that it is only the LIST that is copied. Not the entries.
 * The entries are however referenced, and must be unreffed!
 */
GList *
xmms_playlist_list (xmms_playlist_t *playlist, xmms_error_t *err)
{
	GList *r=NULL, *node;
	XMMS_PLAYLIST_LOCK (playlist);

	for (node = playlist->list; node; node = g_list_next (node)) {
		r = g_list_append (r, GUINT_TO_POINTER (xmms_playlist_entry_id_get ((xmms_playlist_entry_t *)node->data)));
	}

	XMMS_PLAYLIST_UNLOCK (playlist);

	return r;
}


/** initializes a new xmms_playlist_t.
  */

xmms_playlist_t *
xmms_playlist_init (void)
{
	xmms_playlist_t *ret;
	xmms_config_value_t *val;
	const gchar *tmp;

	ret = xmms_object_new (xmms_playlist_t, xmms_playlist_destroy);
	ret->cond = g_cond_new ();
	ret->mutex = g_mutex_new ();
	ret->list = NULL;
	ret->nextid = 1;
	ret->id_table = g_hash_table_new (g_direct_hash, g_direct_equal);
	ret->is_waiting = FALSE;

	xmms_dbus_register_object ("playlist", XMMS_OBJECT (ret));

	xmms_dbus_register_onchange (XMMS_OBJECT (ret),
				     XMMS_SIGNAL_PLAYLIST_MEDIAINFO_ID);
	xmms_dbus_register_onchange (XMMS_OBJECT (ret),
				     XMMS_SIGNAL_PLAYLIST_CHANGED);

	val = xmms_config_value_register ("playlist.mode", "none",
	                                  on_playlist_mode_changed, ret);
	tmp = xmms_config_value_string_get (val);
	ret->mode = playlist_mode_from_str (tmp);

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_SHUFFLE, 
				XMMS_METHOD_FUNC (shuffle));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_JUMP, 
				XMMS_METHOD_FUNC (set_next));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_ADD, 
				XMMS_METHOD_FUNC (add));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_MEDIALIBADD, 
				XMMS_METHOD_FUNC (medialibadd));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_GETMEDIAINFO, 
				XMMS_METHOD_FUNC (getmediainfo));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_REMOVE, 
				XMMS_METHOD_FUNC (remove));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_MOVE, 
				XMMS_METHOD_FUNC (move));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_LIST, 
				XMMS_METHOD_FUNC (list));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_CLEAR, 
				XMMS_METHOD_FUNC (clear));

	xmms_object_method_add (XMMS_OBJECT (ret), 
				XMMS_METHOD_SORT, 
				XMMS_METHOD_FUNC (sort));

	ret->mediainfothr = xmms_mediainfo_thread_start (ret);

	return ret;
}


/** @} */

static xmms_playlist_mode_t
playlist_mode_from_str (const gchar *mode)
{
	if (!mode)
		return XMMS_PLAYLIST_MODE_NONE;

	if (!g_strcasecmp (mode, "repeat_all"))
		return XMMS_PLAYLIST_MODE_REPEAT_ALL;

	return XMMS_PLAYLIST_MODE_NONE;
}

static void
on_playlist_mode_changed (xmms_object_t *object, gconstpointer data,
                          gpointer udata)
{
	xmms_playlist_t *playlist = udata;

	g_mutex_lock (playlist->mutex);
	playlist->mode = playlist_mode_from_str (data);
	g_mutex_unlock (playlist->mutex);
}

/** Free the playlist and other memory in the xmms_playlist_t
 *
 *  This will free all entries in the list!
 */

static void
xmms_playlist_destroy (xmms_object_t *object)
{
	GList *node;
	xmms_playlist_t *playlist = (xmms_playlist_t *)object;
	xmms_config_value_t *val;

	g_return_if_fail (playlist);

	g_cond_free (playlist->cond);
	g_mutex_free (playlist->mutex);

	val = xmms_config_lookup ("playlist.mode");
	xmms_config_value_callback_remove (val, on_playlist_mode_changed);

	xmms_mediainfo_thread_stop (playlist->mediainfothr);

	node = playlist->list;
	
	while (node) {
		xmms_playlist_entry_t *entry = node->data;
		xmms_object_unref (entry);
		node = g_list_next (node);
	}

	g_list_free (playlist->list);
}
