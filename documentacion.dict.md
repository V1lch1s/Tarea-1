<!--
DOCUMENTACIÓN: Hash Table (dict.h)

Esto es una documentación de funciones para <dict.h>

De momento, generada con IA. Puede ser reescrita más adelante según se requiera.
-->

# `dict.h` — Tabla Hash Genérica Ordenable

```c
#ifndef DICT_H
#define DICT_H

#include <stdbool.h>
#include <stddef.h>

/* ═══════════════════════════════════════════════════════════════
 * TIPOS DE FUNCIONES (Callbacks requeridos por el usuario)
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief Función de comparación de claves
 * @param a Puntero a la primera clave (tamaño: key_size)
 * @param b Puntero a la segunda clave (tamaño: key_size)
 * @return 0 si las claves son iguales, valor distinto de 0 si son diferentes
 * @note Debe implementar igualdad lógica para el tipo de clave usado
 */
typedef int (*DictCompareFunc)(const void* a, const void* b);

/**
 * @brief Función de hash para claves
 * @param key Puntero a la clave a hashear (tamaño: key_size)
 * @return Valor de hash no negativo (se aplicará módulo capacity internamente)
 * @note Debe ser determinista: misma clave → mismo hash siempre
 */
typedef size_t (*DictHashFunc)(const void* key);


/* ═══════════════════════════════════════════════════════════════
 * ESTRUCTURAS OPACAS (el usuario no accede a sus miembros)
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief Estructura principal de la tabla hash
 * @note Gestionada internamente; usar solo mediante las funciones públicas
 */
typedef struct Dict Dict;

/**
 * @brief Iterador para recorrer elementos en orden de inserción
 * @note Se obtiene con dict_iterator_create(), se libera con dict_iterator_destroy()
 */
typedef struct DictIterator DictIterator;


/* ═══════════════════════════════════════════════════════════════
 * CONSTRUCTOR Y DESTRUCTOR
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief Crea una nueva tabla hash vacía
 * 
 * @param key_size        Tamaño en bytes de cada clave (ej: sizeof(int))
 * @param value_size      Tamaño en bytes de cada valor (ej: sizeof(char[32]))
 * @param key_compare     Función para comparar claves (obligatoria)
 * @param key_hash        Función para generar hash de claves (obligatoria)
 * @param key_destructor  Función para limpiar recursos de claves (opcional, puede ser NULL)
 * @param value_destructor Función para limpiar recursos de valores (opcional, puede ser NULL)
 * 
 * @return Puntero a Dict nuevo, o NULL si falla la asignación de memoria
 * 
 * @note Los destructores NO deben liberar el puntero recibido; 
 *       solo deben limpiar recursos internos (ej: free(ptr->campo)).
 *       La tabla se encarga de liberar los buffers principales de key/value.
 */
Dict* dict_create(size_t key_size, size_t value_size,
                  DictCompareFunc key_compare,
                  DictHashFunc key_hash,
                  void (*key_destructor)(void*),
                  void (*value_destructor)(void*));

/**
 * @brief Destruye la tabla hash y libera toda su memoria
 * 
 * @param d Puntero a la tabla a destruir (puede ser NULL)
 * 
 * @note Llama a los destructores registrados para cada elemento activo antes de liberar.
 *       Es seguro llamar con NULL (no hace nada).
 */
void dict_destroy(Dict* d);


/* ═══════════════════════════════════════════════════════════════
 * OPERACIONES PRINCIPALES
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief Inserta una nueva clave-valor o actualiza una existente
 * 
 * @param d     Puntero a la tabla hash
 * @param key   Puntero a la clave a insertar/actualizar
 * @param value Puntero al valor asociado
 * 
 * @return true si la operación fue exitosa, false si hay error (NULL, malloc fallido)
 * 
 * @note Si la clave ya existe, se actualiza solo el valor (se preserva el orden de inserción original).
 *       La tabla hace copias internas de key y value; el caller puede liberar sus buffers originales.
 */
bool dict_put(Dict* d, const void* key, const void* value);

/**
 * @brief Obtiene el valor asociado a una clave
 * 
 * @param d         Puntero a la tabla hash (solo lectura)
 * @param key       Puntero a la clave a buscar
 * @param out_value Puntero al buffer donde copiar el valor (puede ser NULL si solo se quiere verificar existencia)
 * 
 * @return true si la clave fue encontrada, false en caso contrario
 * 
 * @note El valor se copia en out_value (tamaño: value_size). 
 *       Si out_value es NULL, solo se verifica la existencia de la clave.
 */
bool dict_get(const Dict* d, const void* key, void* out_value);

/**
 * @brief Elimina una entrada de la tabla hash
 * 
 * @param d   Puntero a la tabla hash
 * @param key Puntero a la clave a eliminar
 * 
 * @return true si la clave fue encontrada y eliminada, false si no existe o error
 * 
 * @note Llama a los destructores registrados antes de liberar la memoria interna.
 *       La eliminación mantiene la integridad del orden de inserción (O(1) gracias a back-pointer).
 */
bool dict_remove(Dict* d, const void* key);

/**
 * @brief Verifica si una clave existe en la tabla
 * 
 * @param d   Puntero a la tabla hash (solo lectura)
 * @param key Puntero a la clave a buscar
 * 
 * @return true si la clave existe y está activa, false en caso contrario
 * 
 * @note Equivalente a dict_get(d, key, NULL) pero más eficiente si no se necesita el valor.
 */
bool dict_contains(const Dict* d, const void* key);

/**
 * @brief Obtiene el número de elementos activos en la tabla
 * 
 * @param d Puntero a la tabla hash (puede ser NULL)
 * 
 * @return Número de elementos almacenados (excluye entradas marcadas como deleted)
 * 
 * @note Retorna 0 si d es NULL (seguro para llamadas defensivas).
 */
size_t dict_size(const Dict* d);

/**
 * @brief Elimina todos los elementos de la tabla, dejándola vacía
 * 
 * @param d Puntero a la tabla hash (puede ser NULL)
 * 
 * @note Llama a los destructores registrados para cada elemento antes de limpiar.
 *       La capacidad interna (capacity) no se reduce; para eso, destruir y recrear.
 *       Es seguro llamar con NULL (no hace nada).
 */
void dict_clear(Dict* d);


/* ═══════════════════════════════════════════════════════════════
 * ITERADOR (Recorrido en orden de inserción)
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief Crea un iterador para recorrer la tabla en orden de inserción
 * 
 * @param d Puntero a la tabla hash (solo lectura)
 * 
 * @return Puntero a DictIterator nuevo, o NULL si falla la asignación o d es NULL
 * 
 * @note El iterador captura el estado actual de la lista de orden.
 *       ⚠️ No modificar la tabla (put/remove/clear) mientras se itera: comportamiento indefinido.
 */
DictIterator* dict_iterator_create(const Dict* d);

/**
 * @brief Avanza el iterador y copia la siguiente clave-valor
 * 
 * @param it        Puntero al iterador
 * @param out_key   Buffer para copiar la clave (puede ser NULL)
 * @param out_value Buffer para copiar el valor (puede ser NULL)
 * 
 * @return true si se obtuvo un elemento, false si se alcanzó el final o error
 * 
 * @note Las copias se realizan con memcpy (tamaños: key_size, value_size).
 *       El iterador se actualiza internamente; llamar repetidamente para recorrer todos los elementos.
 */
bool dict_iterator_next(DictIterator* it, void* out_key, void* out_value);

/**
 * @brief Libera la memoria del iterador
 * 
 * @param it Puntero al iterador a liberar (puede ser NULL)
 * 
 * @note No afecta la tabla hash subyacente. Es seguro llamar con NULL.
 */
void dict_iterator_destroy(DictIterator* it);

#endif /* DICT_H */
```

---

## 📋 Resumen rápido de uso

| Función | Propósito | Complejidad | ¿Modifica la tabla? |
|---------|-----------|-------------|---------------------|
| `dict_create` | Inicializar tabla | O(1) | ✅ (nueva) |
| `dict_destroy` | Liberar recursos | O(n) | ✅ (destruye) |
| `dict_put` | Insertar/Actualizar | O(1) promedio* | ✅ |
| `dict_get` | Consultar valor | O(1) promedio* | ❌ |
| `dict_remove` | Eliminar entrada | O(1) promedio* | ✅ |
| `dict_contains` | Verificar existencia | O(1) promedio* | ❌ |
| `dict_size` | Contar elementos | O(1) | ❌ |
| `dict_clear` | Vaciar tabla | O(n) | ✅ |
| `dict_iterator_create` | Preparar recorrido ordenado | O(1) | ❌ |
| `dict_iterator_next` | Obtener siguiente elemento | O(1) | ❌ |
| `dict_iterator_destroy` | Limpiar iterador | O(1) | ❌ |

> \* **O(1) promedio**: Asume buena función de hash y factor de carga ≤ 0.75. Peor caso: O(n) por colisiones.

---

## ⚠️ Consideraciones importantes

1. **Copias internas**: La tabla siempre hace `memcpy` de claves y valores. El usuario es responsable de gestionar la memoria original.

2. **Destructores**: 
   - Se llaman **antes** de `free()` en los buffers internos.
   - **No deben liberar el puntero recibido**, solo recursos anidados.
   - Ejemplo correcto:
     ```c
     void mi_destructor(void* ptr) {
         MiStruct* s = (MiStruct*)ptr;
         free(s->campo_dinamico);  // ✅ Solo recursos internos
         // ❌ NO: free(s); ← La tabla ya lo hará
     }
     ```

3. **Thread-safety**: Esta implementación **no es thread-safe**. Para uso concurrente, añadir mutex externamente.

4. **Iteración segura**: No modificar la tabla mientras se usa un iterador activo. Si se necesita modificar, primero recolectar claves en un buffer temporal.

5. **NULL safety**: Todas las funciones públicas manejan `NULL` gracefulmente (retornan false/0 o no hacen nada), excepto `dict_create` que requiere callbacks válidos.

---

## 🔍 Ejemplo mínimo de uso

```c
#include "dict.h"
#include <stdio.h>
#include <string.h>

// Helpers para claves int
static size_t int_hash(const void* key) { 
    return *(const int*)key * 2654435761u; 
}
static int int_cmp(const void* a, const void* b) { 
    return (*(const int*)a == *(const int*)b) ? 0 : 1; 
}

int main(void) {
    // Crear tabla: claves int, valores int
    Dict* d = dict_create(sizeof(int), sizeof(int), int_cmp, int_hash, NULL, NULL);
    
    // Insertar
    int k = 42, v = 100;
    dict_put(d, &k, &v);
    
    // Consultar
    int resultado;
    if (dict_get(d, &k, &resultado)) {
        printf("Clave %d → Valor %d\n", k, resultado);  // 42 → 100
    }
    
    // Iterar en orden de inserción
    DictIterator* it = dict_iterator_create(d);
    int clave, valor;
    while (dict_iterator_next(it, &clave, &valor)) {
        printf("Recorrido: %d = %d\n", clave, valor);
    }
    dict_iterator_destroy(it);
    
    // Limpiar
    dict_destroy(d);
    return 0;
}
```