/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 Peter Alm, Tobias Rundstr�m, Anders Gustafsson
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




/**
  * @file vorbisfile decoder.
  * @url http://www.xiph.org/ogg/vorbis/doc/vorbisfile/
  */


#include "xmms/xmms_defs.h"
#include "xmms/xmms_xformplugin.h"
#include "xmms/xmms_sample.h"
#include "xmms/xmms_log.h"
#include "xmms/xmms_medialib.h"

#include <math.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

#include <glib.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct xmms_vorbis_data_St {
	OggVorbis_File vorbisfile;
	ov_callbacks callbacks;
	gint current;
	xmms_audio_format_t *format;
	GMutex *lock;
} xmms_vorbis_data_t;

typedef enum { STRING, INTEGER } ptype;
typedef struct {
	gchar *vname;
	gchar *xname;
	ptype type;
} props;

#define MUSICBRAINZ_VA_ID "89ad4ac3-39f7-470e-963a-56509c546377"

/** These are the properties that we extract from the comments */
static props properties[] = {
	{ "title",                XMMS_MEDIALIB_ENTRY_PROPERTY_TITLE,     STRING  },
	{ "artist",               XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST,    STRING  },
	{ "album",                XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM,     STRING  },
	{ "tracknumber",          XMMS_MEDIALIB_ENTRY_PROPERTY_TRACKNR,   INTEGER },
	{ "date",                 XMMS_MEDIALIB_ENTRY_PROPERTY_YEAR,      STRING  },
	{ "genre",                XMMS_MEDIALIB_ENTRY_PROPERTY_GENRE,     STRING  },
	{ "musicbrainz_albumid",  XMMS_MEDIALIB_ENTRY_PROPERTY_ALBUM_ID,  STRING  },
	{ "musicbrainz_artistid", XMMS_MEDIALIB_ENTRY_PROPERTY_ARTIST_ID, STRING  },
	{ "musicbrainz_trackid",  XMMS_MEDIALIB_ENTRY_PROPERTY_TRACK_ID,  STRING  },
};

/*
 * Function prototypes
 */

static gboolean xmms_vorbis_plugin_setup (xmms_xform_plugin_t *xform_plugin);
static gint xmms_vorbis_read (xmms_xform_t *xform, xmms_sample_t *buf, gint len, xmms_error_t *err);
static gboolean xmms_vorbis_init (xmms_xform_t *decoder);
static void xmms_vorbis_destroy (xmms_xform_t *decoder);
/*
static gboolean xmms_vorbis_seek (xmms_decoder_t *decoder, guint samples);
*/

/*
 * Plugin header
 */

XMMS_XFORM_PLUGIN("vorbis",
                  "Vorbis Decoder", XMMS_VERSION,
                  "Xiph's Ogg/Vorbis decoder",
                  xmms_vorbis_plugin_setup);

static gboolean
xmms_vorbis_plugin_setup (xmms_xform_plugin_t *xform_plugin)
{
	xmms_xform_methods_t methods;

	XMMS_XFORM_METHODS_INIT (methods);
	methods.init = xmms_vorbis_init;
	methods.destroy = xmms_vorbis_destroy;
	methods.read = xmms_vorbis_read;

	xmms_xform_plugin_methods_set (xform_plugin, &methods);

	xmms_xform_plugin_indata_add (xform_plugin,
	                              XMMS_STREAM_TYPE_MIMETYPE,
	                              "application/ogg",
	                              NULL);


	xmms_magic_add ("ogg/vorbis header",
	                "application/ogg",
	                "0 string OggS", ">4 byte 0",
	                ">>28 string \x01vorbis", NULL);

	return TRUE;
}

static void
xmms_vorbis_destroy (xmms_xform_t *xform)
{
	xmms_vorbis_data_t *data;

	g_return_if_fail (xform);

	data = xmms_xform_private_data_get (xform);
	g_return_if_fail (data);

	g_mutex_free (data->lock);

	ov_clear (&data->vorbisfile);
	g_free (data);
}

static size_t
vorbis_callback_read (void *ptr, size_t size, size_t nmemb,
                      void *datasource)
{
	xmms_vorbis_data_t *data;
	xmms_xform_t *xform = datasource;
	xmms_error_t error;
	size_t ret;

	g_return_val_if_fail (xform, 0);

	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, 0);

	ret = xmms_xform_read (xform, ptr, size * nmemb, &error);

	return ret / size;
}

static int
vorbis_callback_seek (void *datasource, ogg_int64_t offset, int whence)
{
	xmms_xform_t *xform = datasource;
	xmms_error_t err;
	gint ret;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	if (whence == SEEK_CUR) {
		whence = XMMS_XFORM_SEEK_CUR;
	} else if (whence == SEEK_SET) {
		whence = XMMS_XFORM_SEEK_SET;
	} else if (whence == SEEK_END) {
		whence = XMMS_XFORM_SEEK_END;
	}

	ret = xmms_xform_seek (xform, (gint64) offset, whence, &err);

	return (ret == -1) ? -1 : 0;
}

static int
vorbis_callback_close (void *datasource)
{
	return 0;
}

static long
vorbis_callback_tell (void *datasource)
{
	xmms_xform_t *xform = datasource;
	xmms_error_t err;

	g_return_val_if_fail (xform, -1);

	xmms_error_reset (&err);

	return xmms_xform_seek (xform, 0, XMMS_XFORM_SEEK_CUR, &err);
}

static void
get_replaygain (xmms_xform_t *xform, vorbis_comment *vc)
{
	const char *tmp = NULL;
	gchar buf[8];

	/* track gain */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_track_gain", 0))) {
		tmp = vorbis_comment_query (vc, "rg_radio", 0);
	}

	if (tmp) {
		g_snprintf (buf, sizeof (buf), "%f",
		            pow (10.0, g_strtod (tmp, NULL) / 20));
		/** @todo this should probably be a int instead? */
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_TRACK, 
		                             buf);
	}

	/* album gain */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_album_gain", 0))) {
		tmp = vorbis_comment_query (vc, "rg_audiophile", 0);
	}

	if (tmp) {
		g_snprintf (buf, sizeof (buf), "%f",
		            pow (10.0, g_strtod (tmp, NULL) / 20));
		/** @todo this should probably be a int instead? */
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_GAIN_ALBUM,
		                             buf);
	}

	/* track peak */
	if (!(tmp = vorbis_comment_query (vc, "replaygain_track_peak", 0))) {
		tmp = vorbis_comment_query (vc, "rg_peak", 0);
	}

	if (tmp) {
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_TRACK,
		                             (gchar *) tmp);
	}

	/* album peak */
	if ((tmp = vorbis_comment_query (vc, "replaygain_album_peak", 0))) {
		xmms_xform_metadata_set_str (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_PEAK_ALBUM,
		                             (gchar *) tmp);
	}
}

static gboolean
xmms_vorbis_init (xmms_xform_t *xform)
{
	xmms_vorbis_data_t *data;
	vorbis_info *vi;
	gint ret;
	guint playtime;
	vorbis_comment *ptr;

	g_return_val_if_fail (xform, FALSE);

	data = g_new0 (xmms_vorbis_data_t, 1),
	data->lock = g_mutex_new ();

	data->callbacks.read_func = vorbis_callback_read;
	data->callbacks.close_func = vorbis_callback_close;
	data->callbacks.tell_func = vorbis_callback_tell;
	data->callbacks.seek_func = vorbis_callback_seek;

	data->current = -1;

	xmms_xform_private_data_set (xform, data);

	ret = ov_open_callbacks (xform, &data->vorbisfile, NULL, 0,
	                         data->callbacks);
	if (ret) {
		return FALSE;
	}

	vi = ov_info (&data->vorbisfile, -1);

	playtime = ov_time_total (&data->vorbisfile, -1);
	if (playtime != OV_EINVAL) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_DURATION,
		                             playtime * 1000);
	}

	if (vi && vi->bitrate_nominal) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_BITRATE,
		                             (gint) vi->bitrate_nominal);
	}

	if (vi && vi->rate) {
		xmms_xform_metadata_set_int (xform,
		                             XMMS_MEDIALIB_ENTRY_PROPERTY_SAMPLERATE,
		                             (gint) vi->rate);
	}

	ptr = ov_comment (&data->vorbisfile, -1);

	if (ptr) {
		gint temp;

		for (temp = 0; temp < ptr->comments; temp++) {
			gchar **s;
			gint i = 0;

			s = g_strsplit (ptr->user_comments[temp], "=", 2);
			for (i = 0; i < G_N_ELEMENTS (properties); i++) {
				if ((g_strcasecmp (s[0], "MUSICBRAINZ_ALBUMARTISTID") == 0) &&
				    (g_strcasecmp (s[1], MUSICBRAINZ_VA_ID) == 0)) {
					xmms_xform_metadata_set_int (xform,
					                             XMMS_MEDIALIB_ENTRY_PROPERTY_COMPILATION, 
					                             1);
				} else if (g_strcasecmp (s[0], properties[i].vname) == 0) {
					if (properties[i].type == INTEGER) {
						gint tmp = strtol (s[1], NULL, 10);
						xmms_xform_metadata_set_int (xform,
						                             properties[i].xname, 
						                             tmp);
					} else {
						xmms_xform_metadata_set_str (xform,
						                             properties[i].xname,
						                             s[1]);
					}
				}
			}

			g_strfreev (s);
		}

		get_replaygain (xform, ptr);
	}

	/*

		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S16,
		                         vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_U16,
		                         vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_S8,
		                         vi->channels, vi->rate);
		xmms_decoder_format_add (decoder, XMMS_SAMPLE_FORMAT_U8,
		                         vi->channels, vi->rate);

		data->format = xmms_decoder_format_finish (decoder);
		if (!data->format) {
			return FALSE;
		}

		XMMS_DBG ("Vorbis inited!!!!");
	}*/

	xmms_xform_outdata_type_add (xform,
	                             XMMS_STREAM_TYPE_MIMETYPE,
	                             "audio/pcm",
	                             XMMS_STREAM_TYPE_FMT_FORMAT,
	                             XMMS_SAMPLE_FORMAT_S16,
	                             XMMS_STREAM_TYPE_FMT_CHANNELS,
	                             vi->channels,
	                             XMMS_STREAM_TYPE_FMT_SAMPLERATE,
	                             vi->rate,
	                             XMMS_STREAM_TYPE_END);


	return TRUE;
}

static gint
xmms_vorbis_read (xmms_xform_t *xform, gpointer buf, gint len, xmms_error_t *err)
{
	gint ret = 0;
	gint c;
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (xform, FALSE);
	
	data = xmms_xform_private_data_get (xform);
	g_return_val_if_fail (data, FALSE);

	g_mutex_lock (data->lock);

	ret = ov_read (&data->vorbisfile, (gchar *) buf, len,
	               G_BYTE_ORDER == G_BIG_ENDIAN,
	               xmms_sample_size_get (XMMS_SAMPLE_FORMAT_S16),
				   1,
	               &c);

	g_mutex_unlock (data->lock);

	if (!ret || ret < 0) {
		return ret;
	}

	/* FIXME: read meta data again!  */
	if (c != data->current) {
		/*
		xmms_vorbis_get_media_info (decoder);
		data->current = c;
		*/
	}

	return ret;
}

/*
static gboolean
xmms_vorbis_seek (xmms_decoder_t *decoder, guint samples)
{
	xmms_vorbis_data_t *data;

	g_return_val_if_fail (decoder, FALSE);

	data = xmms_decoder_private_data_get (decoder);
	g_return_val_if_fail (data, FALSE);

	g_mutex_lock (data->lock);

	if (samples > ov_pcm_total (&data->vorbisfile, -1)) {
		xmms_log_error ("Trying to seek past end of stream");
		g_mutex_unlock (data->lock);
		return FALSE;
	}

	ov_pcm_seek (&data->vorbisfile, samples);

	g_mutex_unlock (data->lock);

	return TRUE;
}
*/

