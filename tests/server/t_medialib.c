#include "xcu.h"

/* nasty hack to make the hack required in waf less nasty */
#include "../src/xmms/error.c"
#include "../src/xmms/utils.c"
#include "../src/xmms/ipc.c"
#include "../src/xmms/object.c"
#include "../src/xmms/config.c"
#include "../src/xmms/medialib.c"

static void xmms_mock_entry (gint tracknr, const gchar *artist,
                             const gchar *album, const gchar *title);
static void xmms_dump (xmmsv_t *value);

#define CU_ASSERT_LIST_INT_EQUAL(list, pos, expected) do { \
		xmmsv_t *item; \
		gint val; \
		CU_ASSERT_EQUAL (XMMSV_TYPE_LIST, xmmsv_get_type (list)) \
			xmmsv_list_get (list, pos, &item); \
		xmmsv_get_int (item, &val); \
		CU_ASSERT_EQUAL (expected, val); \
	} while (0);

SETUP (mlib) {
	g_thread_init (0);
	const gchar *indices[] = {
		XMMS_MEDIALIB_ENTRY_PROPERTY_URL,
		XMMS_MEDIALIB_ENTRY_PROPERTY_STATUS,
		NULL
	};

	/* initialize the global source preferences */
	default_sp = s4_sourcepref_create (source_pref);

	/* initialize the global medialib */
	medialib = xmms_object_new (xmms_medialib_t, NULL);
	medialib->s4 = s4_open (NULL, indices, S4_MEMORY);
	return 0;
}

CLEANUP () {
	s4_sourcepref_unref (default_sp);
	s4_close (medialib->s4);
	xmms_object_unref (medialib);
	return 0;
}


CASE (test_query_ids_order_by_id)
{
	xmmsv_coll_t *universe, *ordered;
	xmmsv_t *meta, *spec, *result;
	xmms_error_t err;

	xmms_error_reset (&err);

	xmms_mock_entry (1, "Vibrasphere", "Lungs for Life", "Decade");
	xmms_mock_entry (2, "Vibrasphere", "Lungs for Life", "Breathing Place");
	xmms_mock_entry (3, "Vibrasphere", "Lungs for Life", "Ensueno (Morning mix)");

	universe = xmmsv_coll_universe ();
	ordered = xmmsv_coll_add_order_operator (universe, "id");

	meta = xmmsv_build_metadata (NULL, xmmsv_new_string ("id"), "first", NULL);
	spec = xmmsv_build_cluster_list (xmmsv_new_string ("_row"), meta);

	result = xmms_medialib_query (ordered, spec, &err);

	CU_ASSERT_LIST_INT_EQUAL (result, 0, 0);
	CU_ASSERT_LIST_INT_EQUAL (result, 1, 1);
	CU_ASSERT_LIST_INT_EQUAL (result, 2, 2);

	xmms_dump (result);
	xmms_dump (spec);

	xmmsv_unref (spec);
	xmmsv_unref (result);
	xmmsv_coll_unref (ordered);
	xmmsv_coll_unref (universe);
}

static void
_xmms_dump_indent (gint indent)
{
	gint i;
	for (i = 0; i < indent; i++)
		printf (" ");
}

static void
_xmms_dump (xmmsv_t *value, gint indent)
{
	gint type = xmmsv_get_type (value);

	switch (type) {
	case XMMSV_TYPE_INT32: {
		gint val;
		xmmsv_get_int (value, &val);
		printf ("%d", val);
		break;
	}
	case XMMSV_TYPE_STRING: {
		const gchar *val;
		xmmsv_get_string (value, &val);
		printf ("%s", val);
		break;
	}
	case XMMSV_TYPE_LIST: {
		xmmsv_list_iter_t *iter;
		xmmsv_get_list_iter (value, &iter);

		printf ("[");
		while (xmmsv_list_iter_valid (iter)) {
			xmmsv_t *item;

			xmmsv_list_iter_entry (iter, &item);

			_xmms_dump (item, indent + 1);

			xmmsv_list_iter_next (iter);
			if (xmmsv_list_iter_valid (iter))
				printf (", ");
		}
		printf ("]");
		break;
	}
	case XMMSV_TYPE_DICT: {
		xmmsv_dict_iter_t *iter;

		xmmsv_get_dict_iter (value, &iter);

		printf ("{\n");
		while (xmmsv_dict_iter_valid (iter)) {
			const gchar *key;
			xmmsv_t *item;

			xmmsv_dict_iter_pair (iter, &key, &item);

			_xmms_dump_indent (indent + 1);
			printf ("%s: ", key);
			_xmms_dump (item, indent + 1);

			xmmsv_dict_iter_next (iter);
			if (xmmsv_dict_iter_valid (iter))
				printf (", ");
			printf ("\n");
		}
		_xmms_dump_indent (indent);
		printf ("}");

		break;
	}
	default:
		printf ("invalid type: %d\n", type);
	}
}

static void
xmms_dump (xmmsv_t *value)
{
	_xmms_dump (value, 0);
	printf ("\n");
}

static void
xmms_mock_entry (gint tracknr, const gchar *artist, const gchar *album, const gchar *title)
{
	xmms_medialib_entry_t entry;
	gchar *path;

	path = g_strconcat (artist, album, title, NULL);
	entry = xmms_medialib_entry_new_encoded (path, NULL);

	xmms_medialib_entry_property_set_int (entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,
	                                      tracknr);
	xmms_medialib_entry_property_set_str (entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,
	                                      artist);
	xmms_medialib_entry_property_set_str (entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,
	                                      album);
	xmms_medialib_entry_property_set_str (entry,
	                                      XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,
	                                      title);

	g_free (path);
}

/* START: Stub some crap so we don't have to pull in the whole daemon */
gboolean
xmms_playlist_remove_by_entry (xmms_playlist_t *playlist,
                               xmms_medialib_entry_t entry)
{
	return TRUE;
}

void
xmms_playlist_insert_entry (xmms_playlist_t *playlist, const gchar *plname,
                            guint32 pos, xmms_medialib_entry_t file, xmms_error_t *err)
{
}

void
xmms_playlist_add_entry (xmms_playlist_t *playlist, const gchar *plname,
                         xmms_medialib_entry_t file, xmms_error_t *err)
{
}

GList *
xmms_xform_browse (const gchar *url, xmms_error_t *error)
{
	return NULL;
}

xmms_mediainfo_reader_t *
xmms_playlist_mediainfo_reader_get (xmms_playlist_t *playlist)
{
	return NULL;
};

void
xmms_mediainfo_reader_wakeup (xmms_mediainfo_reader_t *mr)
{
}
/* END */