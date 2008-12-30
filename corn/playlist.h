#ifndef __playlist_h
#define __playlist_h

#include <glib.h>

extern GArray * playlist;
extern gint playlist_position;
extern gint playlist_mtime;
extern gint playlist_save_wait_time;

#define PLAYLIST_ITEM_N(n) g_array_index(playlist, gchar *, (n))

#define PLAYLIST_CURRENT_ITEM() \
    (playlist->len ? PLAYLIST_ITEM_N(playlist_position) : NULL)

#define PLAYLIST_MTIME_NEVER -1
#define PLAYLIST_MODIFIED() (playlist_mtime != PLAYLIST_MTIME_NEVER)
#define PLAYLIST_FLUSH_TIME_HAS_COME() (PLAYLIST_MODIFIED() && \
    main_time_counter - playlist_mtime >= playlist_save_wait_time)
#define PLAYLIST_MARK_AS_FLUSHED() do { playlist_mtime = PLAYLIST_MTIME_NEVER; } while(0)

#define PLAYLIST_TOUCH() do { \
    if(main_status == CORN_RUNNING) \
        playlist_mtime = main_time_counter; \
    } while(0)

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
