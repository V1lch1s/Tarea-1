#ifndef DICT_H
#define DICT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Dict Dict;

// Funciones de comparación y hash (deben ser compatibles)
typedef int    (*DictCompareFunc)(const void* a, const void* b);
typedef size_t (*DictHashFunc)(const void* key);
/*
 * typedef    size_t    (*DictHashFunc)(const void* key);
 * ^^^^^^     ^^^^^^    ^^^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^
 * │          │         │               └─ parámetro: puntero const void*
 * │          │         └─ nombre del tipo
 * │          └─ retorna size_t (número sin signo)
 * └─ typedef
 */

// Constructor / destructor
Dict* dict_create(size_t key_size, size_t value_size,
                  DictCompareFunc key_compare,
                  DictHashFunc key_hash,
                  void (*key_destructor)(void*),
                  void (*value_destructor)(void*));
void  dict_destroy(Dict* d);

// Operaciones principales
bool          dict_put(Dict* d, const void* key, const void* value);     // Inserción o actualización
bool          dict_get(const Dict* d, const void* key, void* out_value); // Consulta de Valor
bool          dict_remove(Dict* d, const void* key);                     // Eliminación segura
bool          dict_contains(const Dict* d, const void* key);             // Verificacón de clave
size_t        dict_size(const Dict* d);                                  // Número de elmentos activos
void          dict_clear(Dict* d);                                       // Elimina todos los elementos

// Iterador (para recorrido ordenado por inserción)
typedef struct DictIterator DictIterator;

DictIterator* dict_iterator_create(const Dict* d);
bool          dict_iterator_next(DictIterator* it, void* out_key, void* out_value);
void          dict_iterator_destroy(DictIterator* it);

#endif