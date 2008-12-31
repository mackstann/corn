#ifndef __playlist_h
#define __playlist_h

#include <glib.h>

void playlist_init(void);
void playlist_destroy(void);

gint playlist_position(void);
gint playlist_length(void);
gboolean playlist_empty(void);
gchar * playlist_nth(gint i);
gchar * playlist_current(void);

gboolean playlist_modified(void);
gboolean playlist_flush_due(void);
void playlist_mark_as_flushed(void);

void playlist_append(gchar * path);
void playlist_replace_path(const gchar * path);
void playlist_advance(gint how);
void playlist_seek(gint track);
void playlist_clear(void);
void playlist_remove(gint track);
void playlist_move(gint track, gint dest);

#endif
