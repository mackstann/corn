#ifndef __music_metadata_h__
#define __music_metadata_h__

#include "playlist.h"
#include <glib.h>

GHashTable * music_get_playlist_item_metadata(PlaylistItem * item);
GHashTable * music_get_track_metadata(gint track);
GHashTable * music_get_current_track_metadata(void);

#endif

