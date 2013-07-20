#ifndef FAST_CACHE_QUEUE
#define FAST_CACHE_QUEUE

#include <kclangc.h>
#include <pthread.h>

#include "util.c"

typedef struct {
  KCLIST* list;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int size;
} queue;

void queue_del(queue* q){
  kclistdel(*(&q->list));
  pthread_mutex_destroy(&q->mutex);
  pthread_cond_destroy(&q->cond);
}

void queue_init(queue* q){
  *(&q->list) = kclistnew();
  *(&q->size) = 0;
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
}

void queue_add(queue* q, char* value){
  pthread_mutex_lock(&q->mutex);
  kclistpush(*(&q->list), value, strlen(value) + 1);
  *(&q->size) += 1;
  pthread_mutex_unlock(&q->mutex);
  pthread_cond_signal(&q->cond);
}

void queue_get(queue* q, char* value){
  pthread_mutex_lock(&q->mutex);
  while(*(&q->size) == 0){
    pthread_cond_wait(&q->cond, &q->mutex);
  }
  list_shift(*(&q->list), value);
  *(&q->size) -= 1;
  pthread_mutex_unlock(&q->mutex);
}

#endif
