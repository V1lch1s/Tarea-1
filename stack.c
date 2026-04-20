#include "stack.h"
#include <stdlib.h>
#include <string.h>

struct Stack {
	void* data;            // arreglo dinámico
	size_t element_size;
	size_t capacity;
	size_t top;            // índice del tope (próxima posición libre)
	void (*destructor)(void*);
};

Stack* stack_create(size_t element_size, void (*destructor)(void*)) {
	Stack* s = malloc(sizeof(Stack));
	if (!s) return NULL;
	s->element_size = element_size;
	s->capacity = 4;      // capacidad inicial
	s->top = 0;
	s->destructor = destructor;
	s->data = malloc(s->capacity * element_size);
	if (!s->data) {
		free(s);
		return NULL;
	}
	return s;
}

void stack_destroy(Stack* s) {
	if (!s) return;
	// Si hay destructor, llamarlo para cada elemento
	if (s->destructor) {
		char* base = (char*)s->data;
		for (size_t i = 0; i < s->top; ++i) {
				s->destructor(base + i * s->element_size);
		}
	}
	free(s->data);
	free(s);
}

static bool stack_resize(Stack* s, size_t new_capacity) {
	void* new_data = realloc(s->data, new_capacity * s->element_size);
	if (!new_data) return false;
	s->data = new_data;
	s->capacity = new_capacity;
	return true;
}

bool stack_push(Stack* s, const void* element) {
	if (!s || !element) return false;
	if (s->top == s->capacity) {
		if (!stack_resize(s, s->capacity * 2)) return false;
	}
	char* dest = (char*)s->data + s->top * s->element_size;
	memcpy(dest, element, s->element_size);
	s->top++;
	return true;
}

bool stack_pop(Stack* s, void* out) {
	if (!s || stack_is_empty(s)) return false;
	s->top--;
	char* src = (char*)s->data + s->top * s->element_size;
	if (out) memcpy(out, src, s->element_size);
	// Opcional: reducir capacidad si es muy grande
	if (s->top > 0 && s->top == s->capacity / 4 && s->capacity > 4) {
		stack_resize(s, s->capacity / 2);
	}
	return true;
}

bool stack_peek(const Stack* s, void* out) {
	if (!s || stack_is_empty(s) || !out) return false;
	char* src = (char*)s->data + (s->top - 1) * s->element_size;
	memcpy(out, src, s->element_size);
	return true;
}

bool stack_is_empty(const Stack* s) {
	return !s || s->top == 0;
}

size_t stack_size(const Stack* s) {
	return s ? s->top : 0;
}