# Memory buffer

`memory_buffer.h` provides a growable in-memory byte buffer with sequential reads, random-access seeking, and helpers for reading and writing integers in little-endian and big-endian formats.

Define `SHL_MEMORY_BUFFER_IMPLEMENTATION` in exactly one translation unit before including `memory_buffer.h` to compile the implementation.

```c
#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "memory_buffer.h"
```

## Type and macros

| Name | Description |
| --- | --- |
| `memory_buffer_t` | Buffer state with the owned byte range in `data`, the current `length`, and the current cursor in `_pointer`. |
| `mb_e_nd(buffer)` | Returns a pointer to one-past-the-end of the buffer data. |
| `mb_position(buffer)` | Returns the current cursor position measured from the start of the buffer. |

## Lifetime and positioning

| Function | Description | Return type |
| --- | --- | --- |
| `mb_initEmpty`(memory_buffer_t* buffer) | Initializes an empty buffer at position `0`. | `void` |
| `mb_initFromMemory`(memory_buffer_t* buffer, uint8_t* data, size_t length) | Wraps an existing byte range and starts the cursor at the beginning. | `void` |
| `mb_free`(memory_buffer_t* buffer) | Frees the buffer storage and resets the struct. | `void` |
| `mb_getData`(memory_buffer_t* buffer, size_t* length) | Returns a heap-allocated copy of the buffer contents and writes the copied size to `length`. | `uint8_t*` |
| `mb_seek`(memory_buffer_t* buffer, uint32_t position) | Moves the cursor to `position`, growing the buffer if needed. | `bool` |
| `mb_skip`(memory_buffer_t* buffer, int32_t distance) | Moves the cursor relative to the current position. Negative underflow fails. | `bool` |
| `mb_scanTo`(memory_buffer_t* buffer, const void* data, size_t length) | Scans forward until the byte sequence is found. The cursor is left at the start of the match. | `bool` |
| `mb_isEOF`(memory_buffer_t* buffer) | Returns whether the cursor is exactly at the end of the buffer. | `bool` |

## Raw reads and writes

| Function | Description | Return type |
| --- | --- | --- |
| `mb_r_ead`(memory_buffer_t* buffer, uint8_t* value) | Reads one byte. | `bool` |
| `mb_r_eadBytes`(memory_buffer_t* buffer, uint8_t* value, size_t count) | Reads `count` bytes into `value`. | `bool` |
| `mb_r_eadString`(memory_buffer_t* buffer, char* str, size_t count) | Reads `count` raw bytes into `str`. It does not append a null terminator. | `bool` |
| `mb_w_rite`(memory_buffer_t* buffer, uint8_t value) | Writes one byte at the current cursor. | `bool` |
| `mb_w_riteBytes`(memory_buffer_t* buffer, uint8_t values[], size_t count) | Writes `count` bytes from `values`, growing the buffer if needed. | `bool` |
| `mb_w_riteString`(memory_buffer_t* buffer, const char* str, size_t count) | Writes `count` raw bytes from `str`. | `bool` |

## Integer reads

| Function | Description | Return type |
| --- | --- | --- |
| `mb_r_eadInt16LE` / `mb_r_eadInt16BE` | Reads a signed 16-bit integer. | `bool` |
| `mb_r_eadUInt16LE` / `mb_r_eadUInt16BE` | Reads an unsigned 16-bit integer. | `bool` |
| `mb_r_eadInt24LE` / `mb_r_eadInt24BE` | Reads a signed 24-bit value into `int32_t`. | `bool` |
| `mb_r_eadUInt24LE` / `mb_r_eadUInt24BE` | Reads an unsigned 24-bit value into `uint32_t`. | `bool` |
| `mb_r_eadInt32LE` / `mb_r_eadInt32BE` | Reads a signed 32-bit integer. | `bool` |
| `mb_r_eadUInt32LE` / `mb_r_eadUInt32BE` | Reads an unsigned 32-bit integer. | `bool` |

## Integer writes

| Function | Description | Return type |
| --- | --- | --- |
| `mb_w_riteInt16LE` / `mb_w_riteInt16BE` | Writes a signed 16-bit integer. | `bool` |
| `mb_w_riteUInt16LE` / `mb_w_riteUInt16BE` | Writes an unsigned 16-bit integer. | `bool` |
| `mb_w_riteInt24LE` / `mb_w_riteInt24BE` | Writes the low 24 bits of a signed 32-bit value. | `bool` |
| `mb_w_riteUInt24LE` / `mb_w_riteUInt24BE` | Writes the low 24 bits of an unsigned 32-bit value. | `bool` |
| `mb_w_riteInt32LE` / `mb_w_riteInt32BE` | Writes a signed 32-bit integer. | `bool` |
| `mb_w_riteUInt32LE` / `mb_w_riteUInt32BE` | Writes an unsigned 32-bit integer. | `bool` |

Notes:

* Write operations grow the buffer automatically.
* `mb_seek()` can also grow the buffer when seeking past the current end.
* `mb_initFromMemory()` wraps the provided storage directly. Because growth and `mbFree()` use heap allocation, the wrapped memory should be mutable storage that is compatible with `free()` if you plan to grow or free the buffer through this API.

Example:

```c
#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "memory_buffer.h"

int main(void)
{
    memory_buffer_t buffer = {0};
    mbInitEmpty(&buffer);

    mb_w_riteString(&buffer, "DATA", 4);
    mb_w_riteUInt32LE(&buffer, 0x11223344u);

    mbSeek(&buffer, 4);
    uint32_t value = 0;
    mb_r_eadUInt32LE(&buffer, &value);

    mbFree(&buffer);
    return value == 0x11223344u ? 0 : 1;
}
```
