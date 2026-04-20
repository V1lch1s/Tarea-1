#include "queue.h"
#include <stdlib.h>
#include <string.h>

struct Queue {
	void* data;
	size_t element_size;
	size_t capacity;
	size_t front;   // índice del frente
	size_t rear;    // índice del final (próxima posición libre)
	size_t count;   // número de elementos
	void (*destructor)(void*);
};

Queue* queue_create(size_t element_size, void (*destructor)(void*)) {
	Queue* q = malloc(sizeof(Queue));
	if (!q) return NULL;
	q->element_size = element_size;
	q->capacity = 4;
	q->front = 0;
	q->rear = 0;
	q->count = 0;
	q->destructor = destructor;
	q->data = malloc(q->capacity * element_size);
	if (!q->data) {
		free(q);
		return NULL;
	}
	return q;
}

void queue_destroy(Queue* q) {
	if (!q) return;
	if (q->destructor) {
		char* base = (char*)q->data;
		for (size_t i = 0; i < q->count; ++i) {
			size_t idx = (q->front + i) % q->capacity;
			q->destructor(base + idx * q->element_size);
		}
	}
	free(q->data);
	free(q);
}

static bool queue_resize(Queue* q, size_t new_capacity) {
	void* new_data = malloc(new_capacity * q->element_size);
	if (!new_data) return false;
	char* src_base = (char*)q->data;
	char* dst_base = (char*)new_data;
	for (size_t i = 0; i < q->count; ++i) {
			 size_t src_idx = (q->front + i) % q->capacity;
			 memcpy(dst_base + i * q->element_size,
						  src_base + src_idx * q->element_size,
						  q->element_size);
	}
	free(q->data);
	q->data = new_data;
	q->front = 0;
	q->rear = q->count;
	q->capacity = new_capacity;
	return true;
}

bool queue_enqueue(Queue* q, const void* element) {
	if (!q || !element) return false;
	if (q->count == q->capacity) {
		if (!queue_resize(q, q->capacity * 2)) return false;
	}
	char* dest = (char*)q->data + q->rear * q->element_size;
	memcpy(dest, element, q->element_size);
	q->rear = (q->rear + 1) % q->capacity;
	q->count++;
	return true;
}

bool queue_dequeue(Queue* q, void* out) {
	if (!q || queue_is_empty(q)) return false;
	char* src = (char*)q->data + q->front * q->element_size;
	if (out) memcpy(out, src, q->element_size);
	q->front = (q->front + 1) % q->capacity;
	q->count--;
	// Reducción opcional
	if (q->count > 0 && q->count == q->capacity / 4 && q->capacity > 4) {
		queue_resize(q, q->capacity / 2);
	}
	return true;
}

bool queue_peek(const Queue* q, void* out) {
	if (!q || queue_is_empty(q) || !out) return false;
	char* src = (char*)q->data + q->front * q->element_size;
	memcpy(out, src, q->element_size);
	return true;
}

bool queue_is_empty(const Queue* q) {
  return !q || q->count == 0;
}

size_t queue_size(const Queue* q) {
  return q ? q->count : 0;
}