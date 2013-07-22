#ifndef FAST_CACHE_COMMON
#define FAST_CACHE_COMMON
#include "queue.c"

static sig_atomic_t misses = 0;
static sig_atomic_t hits = 0;
static sig_atomic_t connections = 0;

static KCDB* db;
static int server;
static queue requests;

static const char* VERSION = "0.0.1";
const char* STATS_FORMAT =
  "STAT connections %d\r\n"
  "STAT hits %d\r\n"
  "STAT misses %d\r\n"
  "STAT hit_ratio %2.4f\r\n"
  "%s"
  "END\r\n";
#endif
