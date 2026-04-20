#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

// Estructura opaca
typedef struct Stack Stack;

// Constructor / destructor
Stack* stack_create(size_t element_size, void (*destructor)(void*));
void   stack_destroy(Stack* s);

// Operaciones principales
bool  stack_push(Stack* s, const void* element);
bool  stack_pop(Stack* s, void* out);
bool  stack_peek(const Stack* s, void* out);
bool  stack_is_empty(const Stack* s);
size_t stack_size(const Stack* s);

#endif