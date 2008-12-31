#ifndef __playlist_h
#define __playlist_h

#include <glib.h>

extern GArray * playlist;
extern gint playlist_position;
extern gint playlist_mtime;

#define PLAYLIST_ITEM_N(n)      g_array_index(playlist, gchar *, (n))
#define PLAYLIST_CURRENT_ITEM() g_array_index(playlist, gchar *, playlist_position)

void playlist_init(void);
void playlist_destroy(void);
void playlist_append(gchar * path);
void playlist_replace_path(const gchar * path);
void playlist_advance(gint how);
void playlist_seek(gint track);
void playlist_clear(void);
void playlist_remove(gint track);
/* before = -1 to move to the end of the list */
void playlist_move(gint track, gint dest);

gboolean playlist_modified(void);
gboolean playlist_flush_due(void);
void playlist_mark_as_flushed(void);

#endif
