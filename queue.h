#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Queue Queue;

Queue* queue_create(size_t element_size, void (*destructor)(void*));
void   queue_destroy(Queue* q);

bool  queue_enqueue(Queue* q, const void* element);
bool  queue_dequeue(Queue* q, void* out);
bool  queue_peek(const Queue* q, void* out);
bool  queue_is_empty(const Queue* q);
size_t queue_size(const Queue* q);

#endif