/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#ifndef __SIGNAL_XMMS_H__
#define __SIGNAL_XMMS_H__

typedef enum {
	XMMS_OBJECT_CMD_ARG_NONE,
	XMMS_OBJECT_CMD_ARG_UINT32,
	XMMS_OBJECT_CMD_ARG_INT32,
	XMMS_OBJECT_CMD_ARG_STRING,
	XMMS_OBJECT_CMD_ARG_DICT,
	XMMS_OBJECT_CMD_ARG_LIST,
	XMMS_OBJECT_CMD_ARG_PROPDICT,
	XMMS_OBJECT_CMD_ARG_BIN
} xmms_object_cmd_arg_type_t;

typedef enum {
	XMMS_IPC_OBJECT_MAIN,
	XMMS_IPC_OBJECT_PLAYLIST,
	XMMS_IPC_OBJECT_CONFIG,
	XMMS_IPC_OBJECT_OUTPUT,
	XMMS_IPC_OBJECT_MEDIALIB,
	XMMS_IPC_OBJECT_SIGNAL,
	XMMS_IPC_OBJECT_VISUALISATION,
	XMMS_IPC_OBJECT_MEDIAINFO_READER,
	XMMS_IPC_OBJECT_XFORM,
	XMMS_IPC_OBJECT_BINDATA,
	XMMS_IPC_OBJECT_END
} xmms_ipc_objects_t;

typedef enum {
	XMMS_IPC_SIGNAL_OBJECT_DESTROYED,
	XMMS_IPC_SIGNAL_PLAYLIST_CHANGED,
	XMMS_IPC_SIGNAL_CONFIGVALUE_CHANGED,
	XMMS_IPC_SIGNAL_PLAYBACK_STATUS,
	XMMS_IPC_SIGNAL_OUTPUT_VOLUME_CHANGED,
	XMMS_IPC_SIGNAL_OUTPUT_PLAYTIME,
	XMMS_IPC_SIGNAL_OUTPUT_CURRENTID,
	XMMS_IPC_SIGNAL_OUTPUT_OPEN_FAIL,
	XMMS_IPC_SIGNAL_PLAYLIST_CURRENT_POS,
	XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_ADDED,
	XMMS_IPC_SIGNAL_MEDIALIB_ENTRY_UPDATE,
	XMMS_IPC_SIGNAL_MEDIALIB_PLAYLIST_LOADED,
	XMMS_IPC_SIGNAL_TRANSPORT_MIMETYPE,
	XMMS_IPC_SIGNAL_DECODER_THREAD_EXIT,
	XMMS_IPC_SIGNAL_VISUALISATION_DATA,
	XMMS_IPC_SIGNAL_QUIT,
	XMMS_IPC_SIGNAL_MEDIAINFO_READER_STATUS,
	XMMS_IPC_SIGNAL_MEDIAINFO_READER_UNINDEXED,
	XMMS_IPC_SIGNAL_END
} xmms_ipc_signals_t;

typedef enum {
	/* Main */
	XMMS_IPC_CMD_HELLO,
	XMMS_IPC_CMD_QUIT,
	XMMS_IPC_CMD_REPLY,
	XMMS_IPC_CMD_ERROR,
	XMMS_IPC_CMD_PLUGIN_LIST,
	XMMS_IPC_CMD_STATS,

	/* Playlist */
	XMMS_IPC_CMD_SHUFFLE,
	XMMS_IPC_CMD_SET_POS,
	XMMS_IPC_CMD_SET_POS_REL,
	XMMS_IPC_CMD_ADD,
	XMMS_IPC_CMD_ADD_ID,
	XMMS_IPC_CMD_REMOVE,
	XMMS_IPC_CMD_MOVE,
	XMMS_IPC_CMD_LIST,
	XMMS_IPC_CMD_CLEAR,
	XMMS_IPC_CMD_SORT,
	XMMS_IPC_CMD_SAVE,
	XMMS_IPC_CMD_CURRENT_POS,
	XMMS_IPC_CMD_INSERT,
	XMMS_IPC_CMD_INSERT_ID,

	/* Config */
	XMMS_IPC_CMD_GETVALUE,
	XMMS_IPC_CMD_SETVALUE,
	XMMS_IPC_CMD_REGVALUE,
	XMMS_IPC_CMD_LISTVALUES,

	/* output */
	XMMS_IPC_CMD_START,
	XMMS_IPC_CMD_STOP,
	XMMS_IPC_CMD_PAUSE,
	XMMS_IPC_CMD_DECODER_KILL,
	XMMS_IPC_CMD_CPLAYTIME,
	XMMS_IPC_CMD_SEEKMS,
	XMMS_IPC_CMD_SEEKMS_REL,
	XMMS_IPC_CMD_SEEKSAMPLES,
	XMMS_IPC_CMD_SEEKSAMPLES_REL,
	XMMS_IPC_CMD_OUTPUT_STATUS,
	XMMS_IPC_CMD_CURRENTID,
	XMMS_IPC_CMD_VOLUME_SET,
	XMMS_IPC_CMD_VOLUME_GET,

	/* Medialib */
	XMMS_IPC_CMD_INFO,
	XMMS_IPC_CMD_SELECT,
	XMMS_IPC_CMD_PLAYLIST_SAVE_CURRENT,
	XMMS_IPC_CMD_PLAYLIST_LOAD,
	XMMS_IPC_CMD_ADD_TO_PLAYLIST,
	XMMS_IPC_CMD_PLAYLISTS_LIST,
	XMMS_IPC_CMD_PLAYLIST_LIST,
	XMMS_IPC_CMD_PLAYLIST_IMPORT,
	XMMS_IPC_CMD_PLAYLIST_EXPORT,
	XMMS_IPC_CMD_PLAYLIST_REMOVE,
	XMMS_IPC_CMD_PATH_IMPORT,
	XMMS_IPC_CMD_REHASH,
	XMMS_IPC_CMD_GET_ID,
	XMMS_IPC_CMD_PROPERTY_SET_STR,
	XMMS_IPC_CMD_PROPERTY_SET_INT,
	XMMS_IPC_CMD_PROPERTY_REMOVE,

	/* Signal subsystem */
	XMMS_IPC_CMD_SIGNAL,
	XMMS_IPC_CMD_BROADCAST,

	/* xform object */
	XMMS_IPC_CMD_BROWSE,

	/* bindata object */
	XMMS_IPC_CMD_GET_DATA,
	XMMS_IPC_CMD_ADD_DATA,
	XMMS_IPC_CMD_REMOVE_DATA,

	/* end */
	XMMS_IPC_CMD_END
} xmms_ipc_cmds_t;

typedef enum {
	XMMS_PLAYLIST_CHANGED_ADD,
	XMMS_PLAYLIST_CHANGED_INSERT,
	XMMS_PLAYLIST_CHANGED_SHUFFLE,
	XMMS_PLAYLIST_CHANGED_REMOVE,
	XMMS_PLAYLIST_CHANGED_CLEAR,
	XMMS_PLAYLIST_CHANGED_MOVE,
	XMMS_PLAYLIST_CHANGED_SORT
} xmms_playlist_changed_actions_t;

typedef enum {
	XMMS_PLAYBACK_STATUS_STOP,
	XMMS_PLAYBACK_STATUS_PLAY,
	XMMS_PLAYBACK_STATUS_PAUSE
} xmms_playback_status_t;

typedef enum {
	XMMS_MEDIAINFO_READER_STATUS_IDLE,
	XMMS_MEDIAINFO_READER_STATUS_RUNNING
} xmms_mediainfo_reader_status_t;

typedef enum {
	XMMS_PLUGIN_TYPE_ALL,
	XMMS_PLUGIN_TYPE_OUTPUT,
	XMMS_PLUGIN_TYPE_PLAYLIST,
	XMMS_PLUGIN_TYPE_EFFECT,
	XMMS_PLUGIN_TYPE_XFORM
} xmms_plugin_type_t;

#endif /* __SIGNAL_XMMS_H__ */
