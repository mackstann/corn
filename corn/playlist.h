#ifndef __playlist_h
#define __playlist_h

#include <glib.h>

extern GList *playlist;
extern GList *playlist_current;

typedef struct PlaylistItem {
    guint use_path;

    gchar *main_path;

    gchar **paths;
} PlaylistItem;

#define MAIN_PATH(item) (((PlaylistItem*)(item))->main_path)
#define PATH(item) (((PlaylistItem*)(item))->paths \
                    [((PlaylistItem*)(item))->use_path])

#define LISTITEM(it) ((PlaylistItem*)(it->data))

void  playlist_init                 ();
void  playlist_destroy              ();
void  playlist_append_single        (const gchar *path);
void  playlist_append_alternatives  (const gchar *path, gchar *const* alts);
void  playlist_replace_path         (guint num, const gchar *path);
/* re-create the random ordering */
void  playlist_rerandomize          ();
void  playlist_advance              (gint num, gboolean loop);
void  playlist_seek                 (gint num);
void  playlist_clear                ();
void  playlist_remove               (gint num);
/* before = -1 to move to the end of the list */
void  playlist_move                 (gint num, gint before);
void  playlist_next_alternative     ();
void  playlist_fail                 ();
void  playlist_dump                 ();

#endif
