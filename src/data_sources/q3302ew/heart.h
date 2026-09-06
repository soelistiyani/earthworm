#ifndef __HEART_H__
#define __HEART_H__

#include "platform.h"

void message_send( unsigned char, short, char *);
thr_ret Heartbeat( void * );

#endif
