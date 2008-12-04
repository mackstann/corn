#ifndef __fifo_h
#define __fifo_h

#include <glib.h>

gboolean fifo_open       ();
void     fifo_close      ();
void     fifo_destroy    ();

#endif
