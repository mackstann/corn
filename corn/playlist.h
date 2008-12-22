#ifndef __playlist_h
#define __playlist_h

#include <glib.h>

typedef struct PlaylistItem
{
    gchar * main_path;
    GList * paths;
    GList * use_path;
} PlaylistItem;

extern GArray * playlist;
extern gint playlist_position;

#define PLAYLIST_CURRENT_ITEM() \
    (playlist->len ? &g_array_index(playlist, PlaylistItem, playlist_position) \
                   : NULL)

#define MAIN_PATH(item) (((PlaylistItem *)(item))->main_path)
#define PATH(item) ((gchar *)(((PlaylistItem *)(item))->use_path->data))

void playlist_init(void);
void playlist_destroy(void);
void playlist_append(const gchar * path, GList * alts);
void playlist_replace_path(guint track, const gchar * path);
/* re-create the random ordering */
void playlist_rerandomize(void);
void playlist_advance(gint how);
void playlist_seek(gint track);
void playlist_clear(void);
void playlist_remove(gint track);
/* before = -1 to move to the end of the list */
void playlist_move(gint track, gint dest);
void playlist_next_alternative(void);
void playlist_dump(void);
void playlist_fail(void);

#endif
