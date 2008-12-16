#include <glib.h>

// we record songs played in the past and the future so that you can go
// backwards/forwards when random is on, and still get the sense of a playlist
// of sorts, not just "play $RANDOM whenever i hit a button"
//
//                              current song
//                                   |
//                                   |
//                                   v
//  ..---------------------+---------+----------+-------------------------..
//   ...the forgotten past |   past  |  future  | the unrecorded future...
//  ..---------------------+---------+----------+-------------------------..

// past and future each get this many entries
static const gint history_size = 100;

static GQueue * past = NULL;
static GQueue * future = NULL;

void plrand_init(void)
{
    past = g_queue_new();
    future = g_queue_new();
}

void plrand_destroy(void)
{
    g_queue_free(past);
    g_queue_free(future);
}

gint plrand_prev(gint current, gint playlist_len)
{
    g_queue_push_head(future, GINT_TO_POINTER(current));

    if(g_queue_get_length(future) > history_size)
        g_queue_pop_tail(future);

    if(!g_queue_is_empty(past))
        return GPOINTER_TO_INT(g_queue_pop_tail(past));
    else
        return g_random_int_range(0, playlist_len);
}

gint plrand_next(gint current, gint playlist_len)
{
    g_queue_push_tail(past, GINT_TO_POINTER(current));

    if(g_queue_get_length(past) > history_size)
        g_queue_pop_head(past);

    if(!g_queue_is_empty(future))
        return GPOINTER_TO_INT(g_queue_pop_head(future));
    else
        return g_random_int_range(0, playlist_len);
}


