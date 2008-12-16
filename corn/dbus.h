#ifndef __corn_dbus_h__
#define __corn_dbus_h__

#include "cpris-root.h"
#include "mpris-root.h"
#include "mpris-player.h"
#include "mpris-tracklist.h"

extern CprisRoot * cpris_root;
extern MprisRoot * mpris_root;
extern MprisPlayer * mpris_player;
extern MprisTrackList * mpris_tracklist;

int mpris_init(void);
void mpris_destroy(void);

#endif
