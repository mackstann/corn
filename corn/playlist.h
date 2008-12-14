#ifndef __playlist_h
#define __playlist_h

#include <glib.h>

typedef struct PlaylistItem {
    guint use_path;
    gchar *main_path;
    gchar **paths;
} PlaylistItem;

extern GArray * playlist;
extern PlaylistItem * playlist_current;
extern gint playlist_position;

#define PLAYLIST_CURRENT_ITEM() \
    (playlist->len ? &g_array_index(playlist, PlaylistItem, playlist_position) \
                   : NULL)

#define MAIN_PATH(item) (((PlaylistItem*)(item))->main_path)
#define PATH(item) (((PlaylistItem*)(item))->paths \
                    [((PlaylistItem*)(item))->use_path])

void playlist_init(void);
void playlist_destroy(void);
void playlist_append_single(const gchar *path);
void playlist_append_alternatives(const gchar *path, gchar *const* alts);
void playlist_replace_path(guint track, const gchar *path);
/* re-create the random ordering */
void playlist_rerandomize(void);
void playlist_advance(gint num, gboolean loop);
void playlist_seek(gint track);
void playlist_clear(void);
void playlist_remove(gint track);
/* before = -1 to move to the end of the list */
void playlist_move(gint track, gint dest);
void playlist_next_alternative(void);
void playlist_dump(void);
gboolean playlist_fail(void);

#endif
