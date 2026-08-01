

# Bibliotecas de Cabecera Única

Estas son bibliotecas de cabecera única que utilizo en mi código, siguiendo el estilo de las bibliotecas stb de Sean Barrett.

* list.h: Una implementación genérica de lista (ver [list.md](https://github.com/acoto87/shl/blob/master/list.md)).
* stack.h: Una implementación genérica de pila (ver [stack.md](https://github.com/acoto87/shl/blob/master/stack.md)).
* queue.h: Una implementación genérica de cola (ver [queue.md](https://github.com/acoto87/shl/blob/master/queue.md)).
* binary_heap.h: Una implementación genérica de montículo binario (ver [binary_heap.md](https://github.com/acoto87/shl/blob/master/binary_heap.md))
* map.h: Una implementación genérica de tabla hash (ver [map.md](https://github.com/acoto87/shl/blob/master/map.md)).
* set.h: Una implementación genérica de conjunto hash (ver [set.md](https://github.com/acoto87/shl/blob/master/set.md))
* array.h: Una utilidad genérica para trabajar con arrays multidimensionales.
* wstr.h: Vistas de cadena y cadenas en el montículo (ver [wstr.md](https://github.com/acoto87/shl/blob/master/wstr.md)).
* wave_writer.h: Contiene funcionalidades para escribir archivos `.wav` (ver [wave_writer.md](https://github.com/acoto87/shl/blob/master/wave_writer.md)).
* memory_buffer.h: Una implementación de búfer en memoria con acceso aleatorio (ver [memory_buffer.md](https://github.com/acoto87/shl/blob/master/memory_buffer.md)).
* flic.h: Contiene funcionalidades para leer archivos FLIC (ver [flic.md](https://github.com/acoto87/shl/blob/master/flic.md)). Es un puerto a C de la implementación en C++ de la biblioteca FLIC de Aseprite de David Capello: https://github.com/aseprite/flic
* memzone.h: Un asignador de memoria simple. (ver [memzone.md](https://github.com/acoto87/shl/blob/master/memzone.md))
* memzone_audit.h: Cabecera complementaria para memzone.h que registra cada mutación del asignador en un archivo de registro estructurado. (ver [memzone_audit.md](https://github.com/acoto87/shl/blob/master/memzone_audit.md))

Consulta los archivos tests/*_tests.c para ver cómo usarlos.

Están en desarrollo y los utilizo en mis juegos, por lo que úsalos bajo tu propio riesgo.

Cualquier consejo/sugerencia es bienvenido.

## Compilación y ejecución de pruebas

El conjunto de pruebas se compila con [nob](https://github.com/tsoding/nob.h) y utiliza el framework de pruebas C [Unity](https://github.com/ThrowTheSwitch/Unity). Las pruebas se encuentran en `tests/` y cubren cada API pública de cada cabecera.

```sh
cc -std=c99 -Wall -Wextra nob.c -o nob

# Run all tests
./nob test
./nob test all

# Run a single test suite
./nob test wstr_test

# Run under AddressSanitizer
./nob asan
./nob asan array_test

# Run under Valgrind (Linux only)
./nob valgrind
```

## Benchmarks

Los microbenchmarks se encuentran en `benchmarks/` y utilizan [ubench.h](https://github.com/sheredom/ubench.h).
Cada conjunto cubre todas las operaciones principales de la API de forma aislada, escenarios integrados y, cuando corresponda, comparaciones directas con los equivalentes de la stdlib (`malloc` / `realloc` / `free`).

| Suite | Fuente | Qué cubre |
|---|---|---|
| `list_bench` | `benchmarks/list_bench.c` | Todas las operaciones de `list.h`: añadir, insertar, eliminar, buscar, ordenar |
| `memzone_bench` | `benchmarks/memzone_bench.c` | Todas las operaciones de `memzone.h` vs `malloc` / `realloc` / `free` |

```sh
# Build and run all benchmarks
./nob bench

# Build and run a single benchmark suite
./nob bench list_bench
./nob bench memzone_bench
```

> **Nota sobre benchmarks agrupados:** Las operaciones O(1) (por ejemplo, un único alloc+free o un acceso directo a un array) son demasiado cortas para medirse individualmente en Windows sin verse afectadas por el ruido de resolución del temporizador. Esos benchmarks realizan N = 128 operaciones por muestra cronometrada e informan la media agregada; divide entre 128 para obtener el costo por operación.
