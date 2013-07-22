#ifndef FAST_CACHE_QUEUE
#define FAST_CACHE_QUEUE

#include <kclangc.h>
#include <pthread.h>
#include "util.h"

typedef struct {
  KCLIST* list;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int size;
} QUEUE;

typedef QUEUE* PTR_QUEUE;

void queue_del(PTR_QUEUE q);
void queue_init(PTR_QUEUE q);
void queue_add(PTR_QUEUE q, char* value);
void queue_get(PTR_QUEUE q, char* value);
int queue_size(PTR_QUEUE q);

#endif
