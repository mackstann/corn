#include "sockqueue.h"

#include <glib.h>

#include <errno.h>
#include <unistd.h>

sockqueue_t * sockqueue_create(void)
{
    sockqueue_t * q = g_new(sockqueue_t, 1);
    if(!socketpair(AF_UNIX, SOCK_STREAM, 0, q->fd))
        return q;
    g_free(q);
    return NULL;
}

void sockqueue_destroy(sockqueue_t * q)
{
    if(q)
    {
        while(close(q->fd[0]) == -1 && errno == EINTR);
        while(close(q->fd[1]) == -1 && errno == EINTR);
        g_free(q);
    }
}

