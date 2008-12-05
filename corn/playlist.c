#include "config.h"

#include "playlist.h"
#include "music.h"
#include "main.h"
#include "parsefile.h"

GList *playlist = NULL;
GList *playlist_current = NULL;

static GList *playlist_random = NULL;

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
    gint p;

    playlist = g_list_append (playlist, item);

    if (!playlist_current)
        playlist_current = playlist;

    p = g_list_position (playlist_random,
                         g_list_find (playlist_random,
                                      playlist_current->data));
    playlist_random = g_list_insert
        (playlist_random, item, 
         g_random_int_range (p, g_list_length (playlist)) + 1);
}

void
playlist_append_single (const gchar *path)
{
    gchar **paths;

    g_return_if_fail (path != NULL);
    g_return_if_fail (g_utf8_validate (path, -1, NULL));

    if (!parse_file (path)) /* XXX PASS A GERROR**? .. and shit @ client */
        return;

    paths = g_new (gchar*, 2);
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

    /* count 'em */
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

    g_message ("replacing %d with %s", num, path);

    alts = p = LISTITEM (playlist_current)->paths;

    while (alts[nalts]) ++nalts;

    if (num > nalts - 1) {
        g_assert (num == nalts); /* cant be more than one past the end! */
        LISTITEM (playlist_current)->paths =
            g_renew (gchar* ,LISTITEM (playlist_current)->paths, num + 2);
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
        LISTITEM (playlist_current)->paths =
            g_renew (gchar* ,LISTITEM (playlist_current)->paths, num + 2);
}

void
playlist_fail ()
{
    PlaylistItem *item;
    guint nalts = 0;
    static GList *cur = NULL;

    g_return_if_fail (playlist_current != NULL);

    item = LISTITEM (playlist_current);
    while (item->paths[nalts]) ++nalts;
    if (nalts - 1 > item->use_path) {
        ++item->use_path;
        /* try again */
        music_play ();
    } else {
        if (!cur) cur = playlist_current;

        /*playlist_remove (g_list_position (playlist, playlist_current));*/
        playlist_advance (1, main_loop_at_end);
        if (playlist_current != cur)
            music_play (); /* because this can recurse back here, the cur
                              checks prevent an infinate loop, and only allow
                              one loop through the playlist */
        cur = NULL;
    }
}

void
playlist_rerandomize ()
{
    GList *it;
    guint i;

    g_list_free (playlist_random);
    playlist_random = NULL;

    for (i = 1, it = playlist; it != NULL; it = g_list_next (it), ++i)
        playlist_random = g_list_insert (playlist_random, it->data,
                                         g_random_int_range (0, i));
}

void
playlist_advance (gint num, gboolean loop)
{
    static GList *rand_cur = NULL;
    gboolean looped = FALSE;
    gint playing = music_playing;

    if (!playlist) return;

    if(!main_repeat_track)
    {
        if (main_random_order) {
            rand_cur = g_list_find (playlist_random, playlist_current->data);

            while (rand_cur && num > 0) {
                rand_cur = g_list_next (rand_cur);
                if (!rand_cur) {
                    playlist_rerandomize ();
                    rand_cur = playlist_random;
                    looped = TRUE;
                }
                --num;
            }
            while (rand_cur && num < 0) {
                rand_cur = g_list_previous (rand_cur);
                if (!rand_cur) {
                    playlist_rerandomize ();
                    rand_cur = g_list_last (playlist_random);
                    looped = TRUE;
                }
                ++num;
            }
            playlist_current = g_list_find (playlist, rand_cur->data);
        } else {
            while (playlist_current && num > 0) {
                playlist_current = g_list_next (playlist_current);
                if (!playlist_current) {
                    playlist_current = playlist;
                    looped = TRUE;
                }
                --num;
            }
            while (playlist_current && num < 0) {
                playlist_current = g_list_previous (playlist_current);
                if (!playlist_current) {
                    playlist_current = g_list_last (playlist);
                    looped = TRUE;
                }
                ++num;
            }
        }
    }

    music_stop ();
    if ((!looped || loop) && playing == MUSIC_PLAYING)
        music_play ();
}

void
playlist_seek (gint num)
{
    GList *it = g_list_nth (playlist, num);

    if (it) {
        playlist_current = it;

        /* this function is used during load, and we don't want to start
           playing necessarily */
        if (main_status == CORN_RUNNING) {
            music_stop ();
            music_play ();
        }
    }
}

void
playlist_clear ()
{
    while (playlist) {
        listitem_free (playlist->data);
        playlist = g_list_delete_link (playlist, playlist);
    }
    playlist = playlist_current = NULL;
    g_list_free (playlist_random);
    playlist_random = NULL;
    music_stop ();
}

void
playlist_remove (gint num)
{
    GList *it = g_list_nth (playlist, num);
    gint playing = music_playing;

    if (it) {
        if (it == playlist_current) {
            music_stop ();
            playlist_advance (1, main_loop_at_end);
        }
        if (it == playlist_current)
            playlist_current = NULL;
        else if (playing == MUSIC_PLAYING)
            music_play ();

        listitem_free (it->data);
        playlist = g_list_delete_link (playlist, it);

    }
}

void
playlist_move (gint num, gint before)
{
    GList *it = g_list_nth (playlist, num);
    struct PlaylistItem *item;

    if (!it || num == before) return;

    item = it->data;
    if (before > num) ++before;
    playlist = g_list_insert (playlist, item, before);
    playlist = g_list_delete_link (playlist, it);
}

void
playlist_dump ()
{
    GList *it;
    gint i = 1;

    for (it = playlist; it; it = g_list_next(it)) {
        gchar *base = g_path_get_basename (PATH (it->data));
        g_print ("%s %d %s\n",
                 (it->data == playlist_current->data ? ">" : " "), i++, base);
        g_free (base);
    }
}
