// Written by Nick Welch in the year 2008.  Author disclaims copyright.

#include "sockqueue.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

sockqueue_t * sockqueue_create(void)
{
    sockqueue_t * q = malloc(sizeof(sockqueue_t));
    assert(q);
    if(!socketpair(AF_UNIX, SOCK_STREAM, 0, q->fd))
        return q; // success
    // failure
    free(q);
    return NULL;
}

void sockqueue_destroy(sockqueue_t * q)
{
    if(q)
    {
        while(close(q->fd[0]) == -1 && errno == EINTR);
        while(close(q->fd[1]) == -1 && errno == EINTR);
        free(q);
    }
}
