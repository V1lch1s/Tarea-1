// Compilar: gcc tests.c dict.c stack.c queue.c -o tests

/* tests.c */
#include "dict.h"
#include "stack.h"
#include "queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ───────────── Utilidades de Test ───────────── */
static int total_tests = 0;
static int passed_tests = 0;

/* Hago que todo lo que se pone dentro de ASSERT después de `cond`,
 * se acepte como macro variádica.
 * https://gcc.gnu.org/onlinedocs/gcc-3.3.4/cpp/Variadic-Macros.html
 */
#define ASSERT(cond, ...) do {                  \
  total_tests++;                                \
  if (cond) {                                   \
              printf("  ✅ ");                  \
              printf(__VA_ARGS__);               \
              printf("\n");                      \
              passed_tests++;                    \
            } else {                             \
              printf("  ❌ FAIL: ");             \
              printf(__VA_ARGS__);                \
              printf(" (línea %d)\n", __LINE__);  \
            }                                     \
} while(0)  // Solo se ejecuta una vez.
            // Donde sea que se escriba ASSERT(condiición, "mensaje"),
            // el compilador reemplaza por
            // `do { ... } while(0)`

/* Contador global para rastrear llamadas a destructores */
static int destroy_count = 0;
static void track_destructor(void* ptr) { 
  (void)ptr; 
  destroy_count++; 
}

/* ───────────── Helpers para int ───────────── */
static size_t int_hash(const void* key) {
  return *(const int*)key * 2654435761u; /* Multiplicador de Knuth */
}
static int int_compare(const void* a, const void* b) {
  return (*(const int*)a == *(const int*)b) ? 0 : 1;
}

/* ───────────── DICT TESTS ───────────── */
static void test_dict_create_destroy(void) {
  printf("\n[Dict] Creación y Destrucción\n");
  Dict* d = dict_create(sizeof(int), sizeof(int), int_compare, int_hash, NULL, NULL);
  ASSERT(d != NULL, "Creación exitosa");
  ASSERT(dict_size(d) == 0, "Tamaño inicial es 0");
  dict_destroy(d);
  ASSERT(1, "Limpieza sin segfault");
}

static void test_dict_basic_ops(void) {
  printf("\n[Dict] Operaciones Básicas (Put/Get/Contains)\n");
  Dict* d = dict_create(sizeof(int), sizeof(int), int_compare, int_hash, NULL, NULL);

  int k1 = 10, v1 = 100, k2 = 20, v2 = 200, out;
  ASSERT(dict_put(d, &k1, &v1), "Insertar clave 10");
  ASSERT(dict_put(d, &k2, &v2), "Insertar clave 20");
  ASSERT(dict_size(d) == 2, "Tamaño = 2");
  ASSERT(dict_contains(d, &k1), "Contiene clave 10");
  ASSERT(dict_get(d, &k1, &out) && out == 100, "Recuperar valor de 10");
  ASSERT(dict_get(d, &k2, &out) && out == 200, "Recuperar valor de 20");
  ASSERT(!dict_get(d, &(int){99}, &out), "Clave inexistente retorna false");

  dict_destroy(d);
}

static void test_dict_update_with_destructor(void) {
  printf("\n[Dict] Actualización con Destructor (fix buffer temporal)\n");
  destroy_count = 0;
  Dict* d = dict_create(sizeof(int), sizeof(int), int_compare, int_hash, 
                        track_destructor, track_destructor);

  int k = 5, v_old = 50, v_new = 500, out;
  dict_put(d, &k, &v_old);
  ASSERT(destroy_count == 0, "Sin llamadas a destructor en inserción");

  ASSERT(dict_put(d, &k, &v_new), "Actualizar valor existente");
  ASSERT(dict_size(d) == 1, "Tamaño no cambia al actualizar");
  ASSERT(dict_get(d, &k, &out) && out == 500, "Valor actualizado correctamente");
  ASSERT(destroy_count == 1, "Destructor llamado solo para valor viejo");

  dict_destroy(d);
  ASSERT(destroy_count == 3, "Destructores finales: 1 key + 1 val nuevo + 1 val viejo");
}

static void test_dict_remove_and_order(void) {
  printf("\n[Dict] Eliminación y Orden de Inserción\n");
  Dict* d = dict_create(sizeof(int), sizeof(int), int_compare, int_hash, NULL, NULL);

  int keys[] = {1, 2, 3, 4}, vals[] = {10, 20, 30, 40};
  for(int i = 0; i < 4; ++i) dict_put(d, &keys[i], &vals[i]);

  ASSERT(dict_remove(d, &keys[1]), "Eliminar clave 2 (intermedia)");
  ASSERT(dict_size(d) == 3, "Tamaño = 3 tras eliminar");
  ASSERT(!dict_contains(d, &keys[1]), "Clave eliminada ya no existe");

  /* Verificar orden: 1 → 3 → 4 */
  DictIterator* it = dict_iterator_create(d);
  int expected_k[] = {1, 3, 4}, expected_v[] = {10, 30, 40};
  int i = 0, cur_k, cur_v;
  while(dict_iterator_next(it, &cur_k, &cur_v)) {
    ASSERT(cur_k == expected_k[i] && cur_v == expected_v[i], 
            "Iteración mantiene orden tras eliminación");
    i++;
  }
  ASSERT(i == 3, "Iterador recorrió exactamente 3 elementos");
  dict_iterator_destroy(it);
  dict_destroy(d);
}

static void test_dict_resize(void) {
  printf("\n[Dict] Redimensionamiento Automático\n");
  Dict* d = dict_create(sizeof(int), sizeof(int), int_compare, int_hash, NULL, NULL);

  /* Capacidad inicial 8, threshold = 6 → resize al 6º elemento */
  for(int i = 1; i <= 10; ++i) {
      int k = i, v = i * 100;
      ASSERT(dict_put(d, &k, &v), "Insertar para forzar resize");
  }
  ASSERT(dict_size(d) == 10, "Tamaño correcto tras resize");

  /* Verificar integridad post-rehash */
  for(int i = 1; i <= 10; ++i) {
      int k = i, out;
      ASSERT(dict_get(d, &k, &out) && out == i * 100, "Datos intactos tras resize");
  }

  /* Verificar orden preservado */
  DictIterator* it = dict_iterator_create(d);
  for(int i = 1; i <= 10; ++i) {
      int k, v;
      dict_iterator_next(it, &k, &v);
      ASSERT(k == i && v == i * 100, "Orden preservado tras resize");
  }
  dict_iterator_destroy(it);
  dict_destroy(d);
}

static void test_dict_null_safety(void) {
  printf("\n[Dict] NULL Safety\n");
  ASSERT(dict_size(NULL) == 0, "dict_size(NULL) seguro");
  ASSERT(!dict_get(NULL, NULL, NULL), "dict_get(NULL) seguro");
  ASSERT(!dict_remove(NULL, NULL), "dict_remove(NULL) seguro");
  ASSERT(!dict_put(NULL, NULL, NULL), "dict_put(NULL) retorna false");

  Dict* d = dict_create(sizeof(int), sizeof(int), int_compare, int_hash, NULL, NULL);
  ASSERT(!dict_put(d, NULL, NULL), "Put con key/value NULL retorna false");
  dict_destroy(d);
}

/* ───────────── STACK TESTS (LIFO) ───────────── */
static void test_stack_create_destroy(void) {
  printf("\n[Stack] Creación y Destrucción\n");
  Stack* s = stack_create(sizeof(int), NULL);
  ASSERT(s != NULL, "Creación exitosa");
  ASSERT(stack_is_empty(s), "Stack vacío inicialmente");
  ASSERT(stack_size(s) == 0, "Tamaño inicial es 0");
  stack_destroy(s);
  ASSERT(1, "Limpieza sin segfault");
}

static void test_stack_push_pop_lifo(void) {
  printf("\n[Stack] LIFO: Push/Pop\n");
  Stack* s = stack_create(sizeof(int), NULL);

  /* Push: 1, 2, 3 */
  int vals[] = {1, 2, 3};
  for(int i = 0; i < 3; ++i) ASSERT(stack_push(s, &vals[i]), "Push %d", vals[i]);
  ASSERT(stack_size(s) == 3, "Tamaño = 3");

  /* Pop: debe salir 3, 2, 1 (LIFO) */
  int out, expected[] = {3, 2, 1};
  for(int i = 0; i < 3; ++i) {
      ASSERT(stack_pop(s, &out) && out == expected[i], "Pop retorna %d (LIFO)", expected[i]);
  }
  ASSERT(stack_is_empty(s), "Stack vacío tras pops");
  ASSERT(!stack_pop(s, &out), "Pop en stack vacío retorna false");

  stack_destroy(s);
}

static void test_stack_peek(void) {
  printf("\n[Stack] Peek (sin remover)\n");
  Stack* s = stack_create(sizeof(int), NULL);

  int v1 = 10, v2 = 20, out;
  stack_push(s, &v1);
  stack_push(s, &v2);

  ASSERT(stack_peek(s, &out) && out == 20, "Peek retorna último sin remover");
  ASSERT(stack_size(s) == 2, "Tamaño no cambia tras peek");

  stack_pop(s, NULL); /* Remover v2 */
  ASSERT(stack_peek(s, &out) && out == 10, "Peek ahora retorna el anterior");

  stack_destroy(s);
}

static void test_stack_destructor(void) {
  printf("\n[Stack] Destructor en Pop y Destroy\n");
  destroy_count = 0;
  Stack* s = stack_create(sizeof(int), track_destructor);

  for(int i = 0; i < 3; ++i) { int v = i; stack_push(s, &v); }
  ASSERT(destroy_count == 0, "Sin destructores en push");

  int out;
  stack_pop(s, &out);  /* Libera 1 elemento */
  ASSERT(destroy_count == 1, "Destructor llamado en pop");

  stack_destroy(s);    /* Libera 2 restantes */
  ASSERT(destroy_count == 3, "Total destructores: 3 elementos");
}

static void test_stack_null_safety(void) {
  printf("\n[Stack] NULL Safety\n");
  ASSERT(stack_size(NULL) == 0, "stack_size(NULL) seguro");
  ASSERT(stack_is_empty(NULL), "stack_is_empty(NULL) seguro");
  ASSERT(!stack_push(NULL, NULL), "push(NULL) retorna false");
  ASSERT(!stack_pop(NULL, NULL), "pop(NULL) retorna false");
  ASSERT(!stack_peek(NULL, NULL), "peek(NULL) retorna false");
}

/* ───────────── QUEUE TESTS (FIFO) ───────────── */
static void test_queue_create_destroy(void) {
  printf("\n[Queue] Creación y Destrucción\n");
  Queue* q = queue_create(sizeof(int), NULL);
  ASSERT(q != NULL, "Creación exitosa");
  ASSERT(queue_is_empty(q), "Queue vacío inicialmente");
  ASSERT(queue_size(q) == 0, "Tamaño inicial es 0");
  queue_destroy(q);
  ASSERT(1, "Limpieza sin segfault");
}

static void test_queue_enqueue_dequeue_fifo(void) {
  printf("\n[Queue] FIFO: Enqueue/Dequeue\n");
  Queue* q = queue_create(sizeof(int), NULL);

  /* Enqueue: 1, 2, 3 */
  int vals[] = {1, 2, 3};
  for(int i = 0; i < 3; ++i) 
    ASSERT(queue_enqueue(q, &vals[i]), "Enqueue %d", vals[i]);
  ASSERT(queue_size(q) == 3, "Tamaño = 3");

  /* Dequeue: debe salir 1, 2, 3 (FIFO) */
  int out, expected[] = {1, 2, 3};
  for(int i = 0; i < 3; ++i) {
    ASSERT(queue_dequeue(q, &out) && out == expected[i], 
            "Dequeue retorna %d (FIFO)", expected[i]);
  }
  ASSERT(queue_is_empty(q), "Queue vacío tras dequeues");
  ASSERT(!queue_dequeue(q, &out), "Dequeue en queue vacío retorna false");

  queue_destroy(q);
}

static void test_queue_peek(void) {
  printf("\n[Queue] Peek (sin remover)\n");
  Queue* q = queue_create(sizeof(int), NULL);

  int v1 = 10, v2 = 20, out;
  queue_enqueue(q, &v1);
  queue_enqueue(q, &v2);

  ASSERT(queue_peek(q, &out) && out == 10, "Peek retorna primero sin remover");
  ASSERT(queue_size(q) == 2, "Tamaño no cambia tras peek");

  queue_dequeue(q, NULL); /* Remover v1 */
  ASSERT(queue_peek(q, &out) && out == 20, "Peek ahora retorna el siguiente");

  queue_destroy(q);
}

static void test_queue_destructor(void) {
  printf("\n[Queue] Destructor en Dequeue y Destroy\n");
  destroy_count = 0;
  Queue* q = queue_create(sizeof(int), track_destructor);

  for(int i = 0; i < 3; ++i) { int v = i; queue_enqueue(q, &v); }
  ASSERT(destroy_count == 0, "Sin destructores en enqueue");

  int out;
  queue_dequeue(q, &out);  /* Libera 1 elemento */
  ASSERT(destroy_count == 1, "Destructor llamado en dequeue");

  queue_destroy(q);        /* Libera 2 restantes */
  ASSERT(destroy_count == 3, "Total destructores: 3 elementos");
}

static void test_queue_null_safety(void) {
  printf("\n[Queue] NULL Safety\n");
  ASSERT(queue_size(NULL) == 0, "queue_size(NULL) seguro");
  ASSERT(queue_is_empty(NULL), "queue_is_empty(NULL) seguro");
  ASSERT(!queue_enqueue(NULL, NULL), "enqueue(NULL) retorna false");
  ASSERT(!queue_dequeue(NULL, NULL), "dequeue(NULL) retorna false");
  ASSERT(!queue_peek(NULL, NULL), "peek(NULL) retorna false");
}

/* ───────────── CASOS DE ESTRÉS / MIXTOS ───────────── */
static void test_mixed_operations(void) {
  printf("\n[Mixto] Operaciones combinadas\n");

  /* Dict con valores que son punteros a structs simulados */
  typedef struct { int id; char label[8]; } Item;
  destroy_count = 0;

  Dict* d = dict_create(sizeof(int), sizeof(Item), int_compare, int_hash, 
                        NULL, track_destructor);

  for(int i = 0; i < 5; ++i) {
      int k = i;
      Item v = {.id = i * 10, .label = "test"};
      dict_put(d, &k, &v);
  }

  /* Actualizar uno */
  int k_upd = 2;
  Item v_upd = {.id = 999, .label = "new"};
  dict_put(d, &k_upd, &v_upd);
  ASSERT(destroy_count == 1, "Destructor llamado al actualizar");

  /* Eliminar otro */
  dict_remove(d, &(int){0});
  ASSERT(destroy_count == 2, "Destructor llamado al eliminar");

  /* Iterar y verificar orden de inserción (1,2,3,4) */
  DictIterator* it = dict_iterator_create(d);
  int count = 0;
  int tmp_k; Item tmp_v;
  while(dict_iterator_next(it, &tmp_k, &tmp_v)) count++;
  ASSERT(count == 4, "Iterador recorre 4 elementos (5 insertados - 1 eliminado)");
  dict_iterator_destroy(it);

  dict_destroy(d);
  ASSERT(destroy_count == 6, "Destructores finales: 4 valores restantes + 2 previos");
}

static void test_empty_structures(void) {
  printf("\n[Mixto] Operaciones en estructuras vacías\n");

  Stack* s = stack_create(sizeof(int), NULL);
  int out;
  ASSERT(!stack_pop(s, &out), "Pop en stack vacío falla");
  ASSERT(!stack_peek(s, &out), "Peek en stack vacío falla");
  stack_destroy(s);

  Queue* q = queue_create(sizeof(int), NULL);
  ASSERT(!queue_dequeue(q, &out), "Dequeue en queue vacío falla");
  ASSERT(!queue_peek(q, &out), "Peek en queue vacío falla");
  queue_destroy(q);

  Dict* d = dict_create(sizeof(int), sizeof(int), int_compare, int_hash, NULL, NULL);
  ASSERT(!dict_get(d, &(int){1}, &out), "Get en dict vacío falla");
  ASSERT(!dict_remove(d, &(int){1}), "Remove en dict vacío falla");

  DictIterator* it = dict_iterator_create(d);
  ASSERT(!dict_iterator_next(it, NULL, NULL), "Iterador en dict vacío retorna false");
  dict_iterator_destroy(it);
  dict_destroy(d);
}

/* ───────────── Runner Principal ───────────── */
int main(void) {
  printf("🧪 Suite de pruebas: Dict + Stack + Queue\n");
  printf("═══════════════════════════════════════════════\n");

  /* Dict Tests */
  test_dict_create_destroy();
  test_dict_basic_ops();
  test_dict_update_with_destructor();
  test_dict_remove_and_order();
  test_dict_resize();
  test_dict_null_safety();

  /* Stack Tests */
  test_stack_create_destroy();
  test_stack_push_pop_lifo();
  test_stack_peek();
  test_stack_destructor();
  test_stack_null_safety();

  /* Queue Tests */
  test_queue_create_destroy();
  test_queue_enqueue_dequeue_fifo();
  test_queue_peek();
  test_queue_destructor();
  test_queue_null_safety();

  /* Mixed / Stress */
  test_mixed_operations();
  test_empty_structures();

  /* Resumen */
  printf("\n═══════════════════════════════════════════════\n");
  printf("📊 Resultados: %d/%d pruebas pasadas\n", passed_tests, total_tests);

  if (passed_tests == total_tests) {
    printf("🎉 ¡Todas las pruebas exitosas!\n");
    return 0;
  } else {
    printf("⚠️  %d prueba(s) fallaron. Revisar errores arriba.\n", total_tests - passed_tests);
    return 1;
  }
}