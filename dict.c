#include "dict.h"
#include <stdlib.h>
#include <string.h>

/*         Declaraciones anticipadas         */
typedef struct InsertOrderNode InsertOrderNode;
typedef struct DictEntry       DictEntry      ;
typedef struct Dict            Dict           ;

/* Lista doblemente enlazada para mantener el orden de inserción y
 * corregir un error donde las entradas eliminadas permanecían en
 * la lista de orden de inserción.
 */
struct InsertOrderNode {
  void             *key;   // Puntero a la clave (misma dirección que en DictEntry)
  void             *value; // Puntero al valor (misma dirección que en DictEntry)
  InsertOrderNode  *prev;  // Nodo anterior en orden de inserción
  InsertOrderNode  *next;  // Nodo siguiente en orden de inserción
};

struct DictEntry {
  char            *key;        // Copia de la clave (memoria propia)
  char            *value;      // Copia del valor (memoria propia)
  bool             occupied;   // ¿Este slot ha sido usado alguna vez?
  bool             deleted;    // ¿Fue marcado como borrado? (sondeo lineal)
  InsertOrderNode *order_node; // 🔗 Back-pointer al nodo de la lista de orden
};
/*
  Why the back-pointer?
  When you find an entry via hash lookup
  (dict_remove), you need to jump directly
  to its list node to unlink it. Without this
  pointer, you'd have to search the list (O(n)).
*/

struct Dict {
  DictEntry       *entries;                  // Array de buckets (tabla hash)
  size_t           capacity;                 // Número total de buckets
  size_t           size;                     // Elementos activos (no borrados)
  size_t           key_size, value_size;     // Tamaños fijos para copias
  DictCompareFunc  key_compare;              // Función de comparación de claves
  DictHashFunc     key_hash;                 // Función de hash de claves
  void             (*key_destructor)(void*); // Limpieza personalizada de claves
  void           (*value_destructor)(void*); // Limpieza personalizada de valores
  InsertOrderNode *insert_order_head, *insert_order_tail; // Extremos de la lista de orden
};

// Factor de carga máximo
/* α = n / m Donde: n es el número de elementos almacenados
 *                  m es la capacidad de la tabla (número de buckets)
 * 0.75 ofrece el mejor equilibrio: desperdicia solo
 * el 25% de memoria pero mantiene un rendimiento excelente
 */
#define DICT_LOAD_FACTOR 0.75

static size_t dict_hash(const Dict *d, const void *key) {
  return d->key_hash(key) % d->capacity;
}

static bool dict_resize(Dict *d, size_t new_capacity) {
  DictEntry *old_entries  = d->entries;
  size_t     old_capacity = d->capacity;

  // Guardar el estado completo para rollback
  InsertOrderNode *old_head = d->insert_order_head;
  InsertOrderNode *old_tail = d->insert_order_tail;

  DictEntry *new_entries = calloc(new_capacity, sizeof(DictEntry));
  if (!new_entries) {
    // Restaurar el estado anterior en caso de fallo (rollback)
    // Rollback Limpio: Sin cambios en old_entries
    return false;
  }

  // Preparar para migración
  d->entries = new_entries;
  d->capacity = new_capacity;
  d->size = 0;
  d->insert_order_head = d->insert_order_tail = NULL;

  // Reinsertar todas las entradas activas (no borradas)
  for (size_t i = 0; i < old_capacity; ++i) {
    if (old_entries[i].occupied && !old_entries[i].deleted) {
      // Despejar temporalmente los punteros Back-Pointers para evitar confusiones
      InsertOrderNode *temp_node = old_entries[i].order_node;
      old_entries[i].order_node = NULL;
      
      dict_put(d, old_entries[i].key, old_entries[i].value);
      
      // Liberar memoria de las copias viejas (dict_put hizo nuevas)
      free(old_entries[i].key);
      free(old_entries[i].value);
    }
  }

  // Liberar viejas listas de nodos (pero NO key/value porque ya se liberaron arriba)
  InsertOrderNode *node = old_head;
  while (node) {
    InsertOrderNode *next = node->next;
    free(node);  // Solo libera la estructura de nodo
    node = next;
  }
  
  free(old_entries);
  return true;
}

Dict *dict_create(size_t key_size, size_t value_size,
                  DictCompareFunc key_compare,
                  DictHashFunc key_hash,
                  void (*key_destructor)(void*),
                  void (*value_destructor)(void*)) {
  
  Dict *d = malloc(sizeof(Dict));
  
  if (!d) return NULL;

  d->key_size         = key_size;
  d->value_size       = value_size;
  d->key_compare      = key_compare;
  d->key_hash         = key_hash;
  d->key_destructor   = key_destructor;
  d->value_destructor = value_destructor;
  d->capacity         = 8;
  d->size             = 0;
  d->entries          = calloc(d->capacity, sizeof(DictEntry));
  
  if (!d->entries) {
    free(d);
    return NULL;
  }
  
  d->insert_order_head = d->insert_order_tail = NULL;
  return d;
}

static void free_insert_order_list(Dict *d) {
  struct InsertOrderNode *node = d->insert_order_head;
  while (node) {
    InsertOrderNode *next = node->next;
    free(node);
    node = next;
  }
  d->insert_order_head = d->insert_order_tail = NULL;
}

void dict_destroy(Dict *d) {
  if (!d) return;

  // Liberar todas las entradas de la tabla
  for (size_t i = 0; i < d->capacity; ++i) {
    if (d->entries[i].occupied && !d->entries[i].deleted) {
      if (d->key_destructor) d->key_destructor(d->entries[i].key);
      if (d->value_destructor) d->value_destructor(d->entries[i].value);
      free(d->entries[i].key);
      d->entries[i].key = NULL;
      free(d->entries[i].value);
      d->entries[i].value = NULL;
    }
  }
  free(d->entries);
  free_insert_order_list(d);
  free(d);
}

bool dict_put(Dict *d, const void *key, const void *value) {
  if (!d || !key || !value) return false;

  // Redimensionar si es necesario
  if (d->size >= d->capacity * DICT_LOAD_FACTOR) {
    dict_resize(d, d->capacity * 2);
  }

  size_t idx = dict_hash(d, key);
  size_t first_deleted = d->capacity;
  bool is_update = false;
  DictEntry *existing_entry = NULL;

  // Sondeo lineal
  for (size_t i = 0; i < d->capacity; ++i) {
    size_t pos = (idx + i) % d->capacity;
    if (!d->entries[pos].occupied) {
      if (first_deleted == d->capacity && d->entries[pos].deleted) {
        first_deleted = pos;
        continue;
      }
      // Encontramos un slot libre (no ocupado ni borrado)
      if (first_deleted != d->capacity) pos = first_deleted;
      
      // Crear copias de key y value
      char *key_copy = malloc(d->key_size);
      char *val_copy = malloc(d->value_size);

      if (!key_copy || !val_copy) {
        // Liberar solo lo que fue asignado exitosamente
        if (key_copy) free(key_copy);
        if (val_copy) free(val_copy);
        return false;
      }

      memcpy(key_copy, key, d->key_size);
      memcpy(val_copy, value, d->value_size);

      d->entries[pos].key = key_copy;
      d->entries[pos].value = val_copy;
      d->entries[pos].occupied = true;
      d->entries[pos].deleted = false;
      d->size++;

      // Mantener orden de inserción (lista doblemente enlazada)
      InsertOrderNode *node = malloc(sizeof(InsertOrderNode));
      
      if (!node) { free(key_copy); free(val_copy); return false; }
      
      node->key = key_copy;
      node->value = val_copy;
      node->next = NULL;
      node->prev = d->insert_order_tail; // Link hacia Atrás
      
      if (!d->insert_order_tail) {
        d->insert_order_head = d->insert_order_tail = node;
      } else {
        d->insert_order_tail->next = node; // Link hacia Adelante
        d->insert_order_tail = node;
      }

      // Guardar el back-pointer en la entrada del hash (CRITICO!)
      d->entries[pos].order_node = node;
      
      return true;

    } else if (d->key_compare(d->entries[pos].key, key) == 0 &&
                      !d->entries[pos].deleted) {
      // Clave ya existe: ACTUALIZAR valor
      existing_entry = &d->entries[pos];
      is_update = true;
      break;
    }
  }

  if (is_update) {
    // ✅ Copiar primero a buffer temporal para evitar corrupción
    char *temp_copy = malloc(d->value_size);
    if (!temp_copy) return false;
    memcpy(temp_copy, value, d->value_size);
    
    // Destruir viejo valor y reemplazar
    if (d->value_destructor) {
      d->value_destructor(existing_entry->value);
    }
    memcpy(existing_entry->value, temp_copy, d->value_size);
    free(temp_copy);
    return true;
  }

  return false; // No debería ocurrir si redimensionamos bien
}

static DictEntry *dict_find_entry(const Dict *d, const void *key) {
  if (!d || !key) return NULL;
  size_t idx = dict_hash(d, key);
  for (size_t i = 0; i < d->capacity; ++i) {
    size_t pos = (idx + i) % d->capacity;
    if (!d->entries[pos].occupied && !d->entries[pos].deleted)
      break;
    if (d->entries[pos].occupied && !d->entries[pos].deleted &&
      d->key_compare(d->entries[pos].key, key) == 0) {
      return &d->entries[pos];
    }
  }
  return NULL;
}

bool dict_get(const Dict *d, const void *key, void *out_value) {
  DictEntry *e = dict_find_entry(d, key);
  if (!e) return false;
  if (out_value) memcpy(out_value, e->value, d->value_size);
  return true;
}

bool dict_remove(Dict *d, const void *key) {
  DictEntry *e = dict_find_entry(d, key);
  if (!e) return false;
  
  // === UNLINK FROM INSERTION ORDER LIST (O(1)) ===
  InsertOrderNode *node = e->order_node;
  if (node) {
    if (node->prev) {
      node->prev->next = node->next;
    } else {
      // Node was head
      d->insert_order_head = node->next;
    }
    
    if (node->next) {
      node->next->prev = node->prev;
    } else {
      // Node was tail
      d->insert_order_tail = node->prev;
    }
    
    // Free the list node struct itself
    free(node);
  }
  
  // === CLEAN UP HASH ENTRY ===
  e->deleted = true;
  e->occupied = false;
  e->order_node = NULL;  // Clear back-pointer
  
  if (d->key_destructor) d->key_destructor(e->key);
  if (d->value_destructor) d->value_destructor(e->value);
  free(e->key);
  free(e->value);
  
  d->size--;
  return true;
}

/* ✅ No more dangling pointers! The list
 * node is removed *before* the key/value 
 * memory is freed.
 */

bool dict_contains(const Dict *d, const void *key) {
  return dict_find_entry(d, key) != NULL;
}

size_t dict_size(const Dict *d) {
  return d ? d->size : 0;
}

void dict_clear(Dict *d) {
  if (!d) return;
  for (size_t i = 0; i < d->capacity; ++i) {
    if (d->entries[i].occupied && !d->entries[i].deleted) {
      if (d->key_destructor) d->key_destructor(d->entries[i].key);
      if (d->value_destructor) d->value_destructor(d->entries[i].value);
      free(d->entries[i].key);
      d->entries[i].key = NULL;
      free(d->entries[i].value);
      d->entries[i].value = NULL;
    }
    d->entries[i].occupied = false;
    d->entries[i].deleted = false;
  }
  d->size = 0;
  free_insert_order_list(d);
}

// ---------- Iterador en orden de inserción ----------
struct DictIterator {
  struct InsertOrderNode *current;
  size_t key_size;
  size_t value_size;
};

DictIterator *dict_iterator_create(const Dict *d) {
  if (!d) return NULL;
  DictIterator *it = malloc(sizeof(DictIterator));
  if (!it) return NULL;
  it->current = d->insert_order_head;
  it->key_size = d->key_size;
  it->value_size = d->value_size;
  return it;
}

bool dict_iterator_next(DictIterator *it, void *out_key, void *out_value) {
  if (!it || !it->current) return false;
  if (out_key) memcpy(out_key, it->current->key, it->key_size);
  if (out_value) memcpy(out_value, it->current->value, it->value_size);
  it->current = it->current->next;
  return true;
}

void dict_iterator_destroy(DictIterator *it) {
  free(it);
}