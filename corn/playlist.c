#include "config.h"

#include "playlist.h"
#include "music.h"
#include "main.h"
#include "parsefile.h"
#include "configuration.h"
#include "dbus.h"

GQueue * playlist = NULL;
PlaylistItem * playlist_current = NULL;
gint playlist_position = -1;

static GQueue * playlist_random = NULL;

static struct PlaylistItem*
listitem_new(gchar *path, gchar **p, guint nalts)
{
    struct PlaylistItem *s;

    s = g_new (struct PlaylistItem, 1);
    s->main_path = path;
    s->paths = p;
    s->use_path = 0; /*they cycle to the end.. g_random_int_range (0, nalts);*/
    return s;
}

static void
listitem_free (struct PlaylistItem *item)
{
    g_strfreev (item->paths);
    g_free (item->main_path);
    g_free (item);
}

/*
static gboolean
get_file_utf8 (const gchar *path, gchar **f, gchar **u)
{
    if (g_utf8_validate (path, -1, NULL)) {
        if (!(*f = g_filename_from_utf8 (path, -1, NULL, NULL, NULL))) {
            g_critical (_("Skipping '%s'. Could not convert from UTF-8. "
                          "Bug?"), path);
            return FALSE;
        }
        *u = g_strdup (path);
    } else {
        if (!(*u = g_filename_to_utf8 (path, -1, NULL, NULL, NULL))) {
            g_warning (_("Skipping '%s'. Could not convert to UTF-8. "
                         "See the README for a possible solution."), path);
            return FALSE;
        }
        *f = g_strdup (path);
    }
    return TRUE;
}
*/

static void
playlist_append (PlaylistItem *item)
{
    if(!playlist)
    {
        playlist = g_queue_new();
        playlist_random = g_queue_new();
    }

    gint rand_pos_after_current;

    if(!playlist_current)
    {
        playlist_current = item;
        playlist_position = 0;
        rand_pos_after_current = 0;
    }
    else
        rand_pos_after_current = g_random_int_range(
            playlist_position,
            g_queue_get_length(playlist)+1 // +1 because we're about to add a song
        ) + 1; // +1 to put it after the chosen index

    //LOG_TIME("before append");
    g_queue_push_tail(playlist, item);
    //LOG_TIME("after append");
    //LOG_TIME("before rand append");
    g_queue_push_nth(playlist_random, item, rand_pos_after_current);
    //LOG_TIME("after rand append");
}

void
playlist_append_single (const gchar *path)
{
    g_return_if_fail (path != NULL);
    g_return_if_fail (g_utf8_validate (path, -1, NULL));

    if (!parse_file (path)) /* XXX PASS A GERROR**? .. and shit @ client */
        return;

#if 0
#include <xine/compat.h>
    // check against this
        XINE_PATH_MAX
#endif

    gchar ** paths = g_new (gchar*, 2);
    paths[0] = g_strdup (path);
    paths[1] = NULL;

    playlist_append (listitem_new (g_strdup (path), paths, 1));
}

void
playlist_append_alternatives (const gchar *path, gchar *const* alts)
{
    gchar **paths;
    guint i, nalts = 0;

    g_return_if_fail (alts != NULL && alts[0] != NULL);

    while (alts[nalts]) ++nalts;

    paths = g_new (gchar*, nalts + 1);
    paths[nalts] = NULL;

    for (i = 0; i < nalts; ++i) {
        g_return_if_fail (g_utf8_validate (alts[i], -1, NULL));
        paths[i] = g_strdup (alts[i]);
    }

    playlist_append (listitem_new(g_strdup (path), paths, nalts));
}

void
playlist_replace_path (guint num, const gchar *path)
{
    guint i, nalts = 0;
    gchar **p, **alts;

    g_return_if_fail (playlist_current != NULL);

    alts = p = playlist_current->paths;

    while (alts[nalts]) ++nalts;

    if (num > nalts - 1) {
        g_assert (num == nalts); /* cant be more than one past the end! */
        playlist_current->paths =
            g_renew (gchar*, playlist_current->paths, num + 2);
        alts[nalts - 1] = NULL;
        nalts++;
    }

    for (i = 0; i < num; ++i) ++p;
    g_free (*p);
    *p = g_strdup (path);
    for (++p; *p; ++p) {
        g_free (*p);
        *p = NULL;
    }

    if (num < nalts - 1)
        playlist_current->paths =
            g_renew (gchar*, playlist_current->paths, num + 2);
}

gboolean playlist_fail(void)
{
    PlaylistItem *item;
    guint nalts = 0;

    g_return_val_if_fail(playlist_current != NULL, FALSE);

    item = playlist_current;
    while(item->paths[nalts])
        ++nalts;
    if(nalts - 1 > item->use_path) {
        ++item->use_path;
        /* try again */
        return TRUE;
    } else {
        /*playlist_remove (g_list_position (playlist, playlist_current));*/
        playlist_advance(1, config_loop_at_end);
        return TRUE;
    }
    return FALSE;
}

void
playlist_rerandomize ()
{
    g_queue_clear(playlist_random);

    GList * it = g_queue_peek_head_link(playlist);
    for(gint i = 1; it != NULL; it = g_list_next (it), ++i)
        g_queue_push_nth(playlist_random, it->data,
                                         g_random_int_range (0, i));
}

void
playlist_advance (gint num, gboolean loop)
{
    static GList *rand_cur = NULL;
    gboolean looped = FALSE;
    gint wasplaying = music_playing;

    if (!playlist || !num)
        return;

    if(!config_repeat_track)
    {
        if (config_random_order) {
            rand_cur = g_queue_find(playlist_random, playlist_current);

            while (rand_cur && num > 0) {
                rand_cur = g_list_next (rand_cur);
                if (!rand_cur) {
                    playlist_rerandomize ();
                    rand_cur = g_queue_peek_head_link(playlist_random);
                    looped = TRUE;
                }
                --num;
            }
            while (rand_cur && num < 0) {
                rand_cur = g_list_previous (rand_cur);
                if (!rand_cur) {
                    playlist_rerandomize ();
                    rand_cur = g_queue_peek_tail_link(playlist_random);
                    looped = TRUE;
                }
                ++num;
            }
            playlist_position = g_queue_index(playlist, rand_cur->data);
            playlist_current = g_queue_peek_nth(playlist, playlist_position);
        } else {
            GList * cur = g_queue_peek_nth_link(playlist, playlist_position);
            while (cur && num > 0) {
                cur = g_list_next (cur);
                playlist_position++;
                if (!cur) {
                    cur = g_queue_peek_head_link(playlist);
                    playlist_position = 0;
                    looped = TRUE;
                }
                --num;
            }
            while (cur && num < 0) {
                cur = g_list_previous (cur);
                playlist_position--;
                if (!cur) {
                    cur = g_queue_peek_tail_link(playlist);
                    playlist_position = g_queue_get_length(playlist)-1;
                    looped = TRUE;
                }
                ++num;
            }
            playlist_current = cur->data;
        }
    }

    music_stop ();
    if ((!looped || loop) && wasplaying == MUSIC_PLAYING)
        music_play ();

    mpris_player_emit_track_change(mpris_player);
}

void playlist_seek(gint num)
{
    if(num < 0 || num >= g_queue_get_length(playlist))
        return;

    playlist_current = g_queue_peek_nth(playlist, num);
    playlist_position = num;

    /* this function is used during load, and we don't want to start
       playing necessarily */
    if (main_status == CORN_RUNNING) {
        music_stop ();
        // XXX only play if wasplaying
        music_play ();
    }

    mpris_player_emit_track_change(mpris_player);
}

void
playlist_clear ()
{
    music_stop();

    gint len = g_queue_get_length(playlist);
    while(len--)
        listitem_free(g_queue_pop_head(playlist));

    g_queue_free(playlist);
    g_queue_free(playlist_random);

    playlist = playlist_random = NULL;
    playlist_current = NULL;
    playlist_position = -1;
}

void
playlist_remove (gint num)
{
    if(num < 0 || num >= g_queue_get_length(playlist))
        return;

    gint was_playing = music_playing;

    if (num == playlist_position) {
        music_stop ();
        playlist_advance (1, config_loop_at_end);
    }
    if (num == playlist_position)
    {
        playlist_current = NULL;
        playlist_position = -1;
    }
    else if (was_playing == MUSIC_PLAYING)
        music_play ();

    // if we just removed the last song while pointing
    // to it, then point to new end
    if(playlist_position > g_queue_get_length(playlist)-2)
    {
        playlist_position--;
        playlist_current = g_queue_peek_tail(playlist);
    }

    listitem_free(g_queue_pop_nth(playlist, num));
}

void
playlist_move (gint num, gint before)
{
    GList * it = g_queue_peek_nth_link(playlist, num);
    PlaylistItem * item;

    if (!it || num == before) return;

    item = it->data;
    if (before > num) ++before;
    g_queue_push_nth (playlist, item, before);
    g_queue_delete_link (playlist, it);

    if(before <= playlist_position)
        playlist_current = g_queue_peek_nth(playlist, playlist_position);
}

