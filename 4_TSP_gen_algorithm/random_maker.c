#include "random_maker.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

const size_t kRandomQueueSize = 64;
const size_t kRandomQueueBlockSize = 1024;

struct RandomMaker {
  Queue queue_;
  size_t queue_size_;
  pthread_mutex_t mutex_;
  pthread_cond_t cond_consumer_;
  pthread_cond_t cond_producer_;
  atomic_int shutdown_;
  pthread_t thread_;
};

void* GenerateRandomBlock(size_t size) {
  size_t i;
  unsigned* block = (unsigned*)malloc(sizeof(unsigned) * kRandomQueueBlockSize);
  for (i = 0; i < kRandomQueueBlockSize; i++) {
    block[i] = rand();
  }
  return block;
}

void* RandomMakerThreadJob(void* in) {
  RandomMaker* self = (RandomMaker*)in;
  pthread_mutex_lock(&(self->mutex_));
  while (!atomic_load(&(self->shutdown_))) {
    while (self->queue_size_ < kRandomQueueSize) {
      QueuePush(&(self->queue_), GenerateRandomBlock(kRandomQueueBlockSize));
      ++self->queue_size_;
      pthread_mutex_unlock(&(self->mutex_));
      pthread_cond_signal(&(self->cond_consumer_));
      pthread_mutex_lock(&(self->mutex_));
    }
    pthread_cond_wait(&(self->cond_producer_), &(self->mutex_));
  }
  return NULL;
}

RandomMaker* RandomMakerCreate() {
  RandomMaker* self = (RandomMaker*)malloc(sizeof(RandomMaker));
  pthread_mutex_init(&(self->mutex_), NULL);
  QueueInit(&(self->queue_));
  self->queue_size_ = 0;
  pthread_cond_init(&(self->cond_consumer_), NULL);
  pthread_cond_init(&(self->cond_producer_), NULL);
  atomic_store(&(self->shutdown_), 0);
  pthread_create(&(self->thread_), NULL, RandomMakerThreadJob, self);
  return self;
}

void RandomMakerDelete(RandomMaker* self) {
  void* queue_el;
  RandomMakerShutdown(self);
  pthread_cond_signal(&(self->cond_producer_));
  pthread_join(self->thread_, NULL);
  pthread_mutex_destroy(&(self->mutex_));
  pthread_cond_destroy(&(self->cond_producer_));
  pthread_cond_destroy(&(self->cond_consumer_));
  while ((queue_el = QueuePop(&(self->queue_)))) {
    free(queue_el);
  }
  assert(QueueEmpty(&(self->queue_)));
  QueueDestroy(&(self->queue_));
  free(self);
}

unsigned* RandomMakerPopRandom(RandomMaker* self) {
  unsigned* to_return;
  pthread_mutex_lock(&(self->mutex_));
  while (self->queue_size_ == 0) {
    pthread_cond_wait(&(self->cond_consumer_), &(self->mutex_));
  }
  to_return = (unsigned*)QueuePop(&(self->queue_));
  if (--self->queue_size_ != 0) {
    pthread_cond_signal(&(self->cond_consumer_));
  }
  if (self->queue_size_ < kRandomQueueSize / 2) {
    pthread_cond_signal(&(self->cond_producer_));
  }
  pthread_mutex_unlock(&(self->mutex_));
  return to_return;
}

void RandomMakerShutdown(RandomMaker* self) {
  atomic_store(&(self->shutdown_), 1);
  pthread_cond_signal(&(self->cond_producer_));
}

typedef struct RandomBlock {
  unsigned* block;
  size_t cursor;
  size_t length;
  RandomMaker* maker;
} RandomBlock;

RandomBlock* RandomBlockCreate(RandomMaker* maker) {
  RandomBlock* self = (RandomBlock*)malloc(sizeof(RandomBlock));
  self->block = RandomMakerPopRandom(maker);
  self->cursor = 0;
  self->length = kRandomQueueBlockSize;
  self->maker = maker;
  return self;
}

void RandomBlockDelete(RandomBlock* self) {
  free(self->block);
  free(self);
}

unsigned RandomBlockPopRandom(RandomBlock* self) {
  unsigned to_ret = self->block[self->cursor];
  if (++(self->cursor) == self->length) {
    free(self->block);
    self->block = RandomMakerPopRandom(self->maker);
    self->cursor = 0;
  }
  return to_ret;
}

size_t RandomBlockPopRandomLong(RandomBlock* self) {
  size_t result = 0;
  size_t cursor;
  for (cursor = 0; cursor < sizeof(size_t) / sizeof(unsigned); ++cursor) {
    ((unsigned*)&result)[cursor] = RandomBlockPopRandom(self);
  }
  return result;
}

