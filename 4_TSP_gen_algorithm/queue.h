struct QueueNode;

typedef struct Queue {
  struct QueueNode* top_;
  struct QueueNode* back_;
} Queue;

void QueueInit(Queue* self);

void QueueDestroy(Queue* self);

void* QueueTop(const Queue* self);

void* QueuePop(Queue* self);

void QueuePush(Queue* self, void* value);

int QueueEmpty(Queue* self);
