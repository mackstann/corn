#ifndef __music_metadata_h__
#define __music_metadata_h__

#include "playlist.h"

#include <glib.h>

GHashTable * music_get_playlist_item_metadata(const gchar * item);
GHashTable * music_get_track_metadata(gint track);
GHashTable * music_get_current_track_metadata(void);

void add_metadata_from_int(GHashTable * meta, const gchar * name, gint num);
void add_metadata_from_string(GHashTable * meta, const gchar * name, const gchar * str);

#endif
