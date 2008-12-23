#ifndef __playlist_h
#define __playlist_h

#include <glib.h>

extern GArray * playlist;
extern gint playlist_position;

#define PLAYLIST_ITEM_N(n) g_array_index(playlist, gchar *, (n))

#define PLAYLIST_CURRENT_ITEM() \
    (playlist->len ? PLAYLIST_ITEM_N(playlist_position) : NULL)

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

#endif
