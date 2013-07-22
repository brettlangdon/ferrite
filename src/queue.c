#include "queue.h"

void queue_del(PTR_QUEUE q){
  kclistdel(*(&q->list));
  pthread_mutex_destroy(&q->mutex);
  pthread_cond_destroy(&q->cond);
}

void queue_init(PTR_QUEUE q){
  *(&q->list) = kclistnew();
  *(&q->size) = 0;
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
}

void queue_add(PTR_QUEUE q, char* value){
  pthread_mutex_lock(&q->mutex);
  kclistpush(*(&q->list), value, strlen(value) + 1);
  *(&q->size) += 1;
  pthread_mutex_unlock(&q->mutex);
  pthread_cond_signal(&q->cond);
}

void queue_get(PTR_QUEUE q, char* value){
  pthread_mutex_lock(&q->mutex);
  while(*(&q->size) == 0){
    pthread_cond_wait(&q->cond, &q->mutex);
  }
  list_shift(*(&q->list), value);
  *(&q->size) -= 1;
  pthread_mutex_unlock(&q->mutex);
}

int queue_size(PTR_QUEUE q){
  int size = 0;
  pthread_mutex_lock(&q->mutex);
  size = *(&q->size);
  pthread_mutex_unlock(&q->mutex);
  return size;
}
