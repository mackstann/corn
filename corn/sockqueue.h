// Written by Nick Welch in the year 2008.  Author disclaims copyright.

#ifndef __corn_sockqueue_h__
#define __corn_sockqueue_h__

#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>

typedef struct _sockqueue_t {
    int fd[2];
} sockqueue_t;

#define __sockqueue_read_or_write(func, fd, type, data) do { \
        ssize_t ntransferred = 0; \
        do { \
            ssize_t ret = func((fd), \
                    (void *) (((char *)(data)) + ntransferred), \
                    sizeof(type) - ntransferred); \
            if(ret == -1) \
                assert(ret == EINTR || ret == EAGAIN); \
            else \
                ntransferred += ret; \
        } while(ntransferred < sizeof(type)); \
    } while(0)

#define sockqueue_read( fd, type, data) do { __sockqueue_read_or_write(read,  fd, type, data); } while(0)
#define sockqueue_write(fd, type, data) do { __sockqueue_read_or_write(write, fd, type, data); } while(0)

sockqueue_t * sockqueue_create(void);
void sockqueue_destroy(sockqueue_t * q);

#endif
