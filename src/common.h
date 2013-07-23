#ifndef FAST_CACHE_COMMON
#define FAST_CACHE_COMMON
#include "queue.h"

extern sig_atomic_t misses;
extern sig_atomic_t hits;
extern sig_atomic_t connections;
extern int record_expire_time;
KCDB* db;
int server;
QUEUE requests;
#endif
