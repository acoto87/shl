# Memory buffer

`memory_buffer.h` provides an owning, growable byte buffer with:

- sequential reads and writes;
- random-access reads and writes;
- explicit cursor positioning;
- explicit capacity and logical-size management;
- transactional failure behavior;
- endian-aware signed and unsigned integer helpers;
- strict signed and unsigned 24-bit support;
- allocator hooks for custom allocators and deterministic failure tests.

The implementation is C99-compatible and can be included from C++ through its
`extern "C"` declarations.

## Integration

Include the declarations wherever they are needed:

```c
#include "memory_buffer.h"
```

In exactly one translation unit, define the implementation macro first:

```c
#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "memory_buffer.h"
```

The structure is public so it can be stack-allocated:

```c
typedef struct memory_buffer_t
{
    uint8_t* data;
    size_t length;
    size_t capacity;
    size_t position;
} memory_buffer_t;
```

The recommended initialization style is:

```c
memory_buffer_t buffer = {0};
mb_initEmpty(&buffer);
```

A zero-initialized `memory_buffer_t` is already a valid empty buffer.

## Why the structure has four fields

`data`, `length`, and `position` are enough for a fixed-size memory view. A
*growable owning buffer* also needs `capacity`.

- `length` is the number of initialized bytes that belong to the logical buffer.
- `capacity` is the number of allocated bytes available in `data`.
- `position` is the cursor used by sequential reads and writes.

Keeping `length` and `capacity` separate avoids reallocating on every append and
lets `mb_reserve` grow storage without changing logical contents.

The invariants are:

```text
position <= length <= capacity
capacity == 0  => data == NULL
capacity > 0   => data != NULL
```

Applications should treat the fields as readable state. Directly modifying them
can violate the invariants and makes subsequent behavior undefined at the API
contract level.

## Ownership model

`memory_buffer_t` is always an owning buffer.

There is no ambiguous borrowed-storage mode:

| Initializer | Storage behavior |
| --- | --- |
| `mb_initEmpty` | Creates an empty owning buffer. |
| `mb_initWithCapacity` | Allocates owned capacity with logical length zero. |
| `mb_initFromMemory` | Copies the supplied bytes. The caller retains ownership of the source. |
| `mb_initTakeOwnership` | Explicitly transfers an existing allocation to the buffer. |

This prevents accidental attempts to `free` stack, static, or externally owned
memory.

### Copy initialization

```c
const uint8_t packet[] = {0x01, 0x02, 0x03};
memory_buffer_t buffer = {0};

if (!mb_initFromMemory(&buffer, packet, sizeof(packet)))
{
    /* allocation failure or invalid arguments */
}
```

`mb_initFromMemory` returns `true` for `(NULL, 0)` and returns `false` for a null
source with a nonzero length.

### Ownership transfer

```c
uint8_t* allocation = malloc(1024);
memory_buffer_t buffer = {0};

if (!mb_initTakeOwnership(&buffer, allocation, 100, 1024))
{
    free(allocation); /* ownership was not transferred */
}
```

The transferred allocation must be compatible with
`SHL_MEMORY_BUFFER_FREE`. `length` must not exceed `capacity`.

### Detaching storage

```c
size_t length = 0;
size_t capacity = 0;
uint8_t* allocation = mb_detach(&buffer, &length, &capacity);

/* buffer is now empty; caller owns allocation */
free(allocation);
```

## Allocator customization

By default, the implementation uses `malloc`, `realloc`, and `free`.

Override them before including the implementation:

```c
#define SHL_MEMORY_BUFFER_MALLOC(size) my_malloc(size)
#define SHL_MEMORY_BUFFER_REALLOC(pointer, size) my_realloc(pointer, size)
#define SHL_MEMORY_BUFFER_FREE(pointer) my_free(pointer)

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "memory_buffer.h"
```

The initial geometric-growth target defaults to 64 bytes. It can be changed:

```c
#define SHL_MEMORY_BUFFER_INITIAL_CAPACITY 256u
```

All allocator macros used by one implementation translation unit must belong to
the same allocator family.

## Failure guarantees

The API uses transactional behavior where practical.

When an operation returns `false`:

- failed reads do not change the cursor;
- failed reads do not change the destination value or destination bytes;
- failed searches do not change the cursor;
- failed seeks and skips do not change the cursor;
- failed writes caused by invalid input, overflow, or allocation failure do not
  change `data`, `length`, `capacity`, `position`, or existing contents;
- failed resize and reserve operations preserve the previous buffer state.

`mb_free`, `mb_clear`, `mb_rewind`, and `mb_initEmpty` return no status.

## Zero-length operations

The following are valid and succeed without dereferencing their pointer
arguments:

```c
mb_readBytes(&buffer, NULL, 0);
mb_writeBytes(&buffer, NULL, 0);
mb_scanTo(&buffer, NULL, 0);
```

A zero-length scan matches at the current position.

`mb_data` returns `NULL` for an empty buffer and reports length zero.

## Lifetime functions

### `mb_initEmpty`

```c
void mb_initEmpty(memory_buffer_t* buffer);
```

Resets the fields to the canonical empty state. It does not free an existing
allocation. Use `mb_free` before reinitializing a populated buffer.

### `mb_initWithCapacity`

```c
bool mb_initWithCapacity(memory_buffer_t* buffer, size_t initial_capacity);
```

Creates an empty buffer and reserves at least `initial_capacity` bytes.

### `mb_initFromMemory`

```c
bool mb_initFromMemory(
    memory_buffer_t* buffer,
    const void* data,
    size_t length);
```

Copies `length` bytes and starts the cursor at zero.

This differs from the old implementation: the source is no longer adopted or
freed by the buffer.

### `mb_initTakeOwnership`

```c
bool mb_initTakeOwnership(
    memory_buffer_t* buffer,
    uint8_t* data,
    size_t length,
    size_t capacity);
```

Explicitly transfers ownership and starts the cursor at zero.

### `mb_free`

```c
void mb_free(memory_buffer_t* buffer);
```

Frees owned storage and resets all fields. It is safe to call with `NULL`, on an
empty buffer, or repeatedly after a successful free.

### `mb_data`

```c
uint8_t* mb_data(const memory_buffer_t* buffer, size_t* length);
```

Returns an allocated copy of the logical contents. The caller owns the returned
allocation. With the default allocator it can be released with `free`.

The return value is `NULL` for an empty buffer or allocation failure. In either
case, `*length` is set to zero when `length` is not null.

### Direct data access

```c
const uint8_t* mb_constData(const memory_buffer_t* buffer);
uint8_t* mb_mutableData(memory_buffer_t* buffer);
```

These functions expose the current allocation without copying it.

Any operation that grows or shrinks the allocation can invalidate previously
returned pointers. Do not retain them across `mb_reserve`, growing writes,
`mb_resize`, `mb_shrinkToFit`, `mb_free`, or `mb_detach`.

## Capacity and logical length

### `mb_reserve`

```c
bool mb_reserve(memory_buffer_t* buffer, size_t minimum_capacity);
```

Ensures that `capacity >= minimum_capacity`. It does not change `length`,
`position`, or logical bytes.

Capacity grows geometrically rather than reallocating to the exact size on every
append.

### `mb_resize`

```c
bool mb_resize(memory_buffer_t* buffer, size_t new_length);
```

Changes logical length.

- Growth zero-initializes every newly exposed byte.
- Shrinking retains capacity.
- If the cursor is beyond the new end, it is clamped to `new_length`.

Use `mb_resize` rather than seeking to create a larger logical buffer.

### `mb_shrinkToFit`

```c
bool mb_shrinkToFit(memory_buffer_t* buffer);
```

Attempts to reduce capacity to exactly `length`. For an empty buffer it releases
the allocation and sets capacity to zero.

Failure leaves the original allocation unchanged.

### `mb_clear`

```c
void mb_clear(memory_buffer_t* buffer);
```

Sets `length` and `position` to zero while preserving capacity for reuse.

### `mb_rewind`

```c
void mb_rewind(memory_buffer_t* buffer);
```

Sets only `position` to zero.

## Positioning

### `mb_seek`

```c
bool mb_seek(memory_buffer_t* buffer, size_t position);
```

Moves the cursor to an absolute position in the inclusive range
`[0, length]`.

Seeking beyond `length` fails and does not allocate. This is intentionally
stricter than the old implementation, where seek could grow the buffer.

### `mb_skip`

```c
bool mb_skip(memory_buffer_t* buffer, ptrdiff_t distance);
```

Moves relative to the current cursor. Both underflow before zero and overflow
past `length` fail transactionally. `PTRDIFF_MIN` is handled without negating it.

### Accessors

```c
size_t mb_position(const memory_buffer_t* buffer);
size_t mb_length(const memory_buffer_t* buffer);
size_t mb_capacity(const memory_buffer_t* buffer);
size_t mb_remaining(const memory_buffer_t* buffer);
uint8_t* mb_end(memory_buffer_t* buffer);
const uint8_t* mb_constEnd(const memory_buffer_t* buffer);
bool mb_isEOF(const memory_buffer_t* buffer);
```

`mb_end` and `mb_constEnd` return `NULL` for an empty buffer whose `data` pointer
is null.

## Searching

```c
bool mb_scanTo(
    memory_buffer_t* buffer,
    const void* data,
    size_t length);
```

Searches forward from the cursor for an exact byte sequence.

- Success places the cursor at the first byte of the match.
- Failure leaves the cursor unchanged.
- Binary patterns containing zero bytes are supported.
- A zero-length pattern succeeds at the current cursor.
- A null pattern is invalid when `length > 0`.

The matched bytes are not consumed. A subsequent read starts at the beginning of
the match.

## Raw reads

```c
bool mb_read(memory_buffer_t* buffer, uint8_t* value);
bool mb_readBytes(memory_buffer_t* buffer, void* values, size_t count);
bool mb_readString(memory_buffer_t* buffer, char* str, size_t count);
```

Sequential reads consume bytes by advancing `position`.

`mb_readString` reads raw bytes and does not append a null terminator.

```c
char text[5] = {0};
if (mb_readString(&buffer, text, 4))
{
    /* text is null-terminated because the destination was pre-zeroed */
}
```

A read past the logical end fails before copying anything.

### Non-consuming reads

```c
bool mb_peekBytes(
    const memory_buffer_t* buffer,
    void* values,
    size_t count);

bool mb_readAt(
    const memory_buffer_t* buffer,
    size_t position,
    void* values,
    size_t count);
```

These functions do not modify the cursor.

## Raw writes

```c
bool mb_write(memory_buffer_t* buffer, uint8_t value);
bool mb_writeBytes(memory_buffer_t* buffer, const void* values, size_t count);
bool mb_writeString(memory_buffer_t* buffer, const char* str, size_t count);
bool mb_writeZeros(memory_buffer_t* buffer, size_t count);
```

Sequential writes begin at `position`:

- bytes inside the existing length are overwritten;
- bytes written at the end are appended;
- capacity grows automatically when needed;
- `length` becomes the furthest written position;
- `position` advances by the number of written bytes.

The source may refer to initialized bytes inside the same buffer. When a growth operation could invalidate an internal source pointer, the source
bytes are staged before reallocation. Copying otherwise uses `memmove` semantics.
As with `memcpy`/`memmove`, the caller must supply a source range containing at
least `count` readable bytes.

### Random-access writes

```c
bool mb_writeAt(
    memory_buffer_t* buffer,
    size_t position,
    const void* values,
    size_t count);
```

Writes at an absolute position without modifying the cursor. `position` must be
within `[0, length]`. Writing at `length` appends. Writing beyond `length` fails;
use `mb_resize` first when a zero-filled gap is desired.

## Integer encoding

Integer helpers encode explicitly and do not depend on host byte order,
alignment, type-punning, or packed structures.

All signed serialized values use two's-complement bit patterns. Decoding avoids
implementation-defined conversions from out-of-range unsigned values to signed
values.

### 16-bit functions

```c
bool mb_readInt16LE(memory_buffer_t*, int16_t*);
bool mb_readInt16BE(memory_buffer_t*, int16_t*);
bool mb_readUInt16LE(memory_buffer_t*, uint16_t*);
bool mb_readUInt16BE(memory_buffer_t*, uint16_t*);

bool mb_writeInt16LE(memory_buffer_t*, int16_t);
bool mb_writeInt16BE(memory_buffer_t*, int16_t);
bool mb_writeUInt16LE(memory_buffer_t*, uint16_t);
bool mb_writeUInt16BE(memory_buffer_t*, uint16_t);
```

### 24-bit functions

```c
bool mb_readInt24LE(memory_buffer_t*, int32_t*);
bool mb_readInt24BE(memory_buffer_t*, int32_t*);
bool mb_readUInt24LE(memory_buffer_t*, uint32_t*);
bool mb_readUInt24BE(memory_buffer_t*, uint32_t*);

bool mb_writeInt24LE(memory_buffer_t*, int32_t);
bool mb_writeInt24BE(memory_buffer_t*, int32_t);
bool mb_writeUInt24LE(memory_buffer_t*, uint32_t);
bool mb_writeUInt24BE(memory_buffer_t*, uint32_t);
```

Ranges are strict:

```text
signed:   -8388608 through 8388607
unsigned: 0 through 16777215
```

Out-of-range writes return `false` and write nothing. Signed reads perform proper
sign extension; `FF FF FF` decodes as `-1`.

### 32-bit functions

```c
bool mb_readInt32LE(memory_buffer_t*, int32_t*);
bool mb_readInt32BE(memory_buffer_t*, int32_t*);
bool mb_readUInt32LE(memory_buffer_t*, uint32_t*);
bool mb_readUInt32BE(memory_buffer_t*, uint32_t*);

bool mb_writeInt32LE(memory_buffer_t*, int32_t);
bool mb_writeInt32BE(memory_buffer_t*, int32_t);
bool mb_writeUInt32LE(memory_buffer_t*, uint32_t);
bool mb_writeUInt32BE(memory_buffer_t*, uint32_t);
```

Byte values are converted to `uint32_t` before shifting. Values such as
`0x89ABCDEF` therefore do not trigger signed-left-shift undefined behavior under
UBSan.

### 64-bit functions

```c
bool mb_readInt64LE(memory_buffer_t*, int64_t*);
bool mb_readInt64BE(memory_buffer_t*, int64_t*);
bool mb_readUInt64LE(memory_buffer_t*, uint64_t*);
bool mb_readUInt64BE(memory_buffer_t*, uint64_t*);

bool mb_writeInt64LE(memory_buffer_t*, int64_t);
bool mb_writeInt64BE(memory_buffer_t*, int64_t);
bool mb_writeUInt64LE(memory_buffer_t*, uint64_t);
bool mb_writeUInt64BE(memory_buffer_t*, uint64_t);
```

## Complete example

```c
#include <stdint.h>
#include <stdio.h>

#define SHL_MEMORY_BUFFER_IMPLEMENTATION
#include "memory_buffer.h"

int main(void)
{
    memory_buffer_t buffer = {0};
    uint16_t version;
    int32_t coordinate;
    uint32_t identifier;

    if (!mb_writeUInt16LE(&buffer, 3u) ||
        !mb_writeInt24BE(&buffer, -120000) ||
        !mb_writeUInt32BE(&buffer, 0x89ABCDEFu))
    {
        mb_free(&buffer);
        return 1;
    }

    mb_rewind(&buffer);

    if (!mb_readUInt16LE(&buffer, &version) ||
        !mb_readInt24BE(&buffer, &coordinate) ||
        !mb_readUInt32BE(&buffer, &identifier))
    {
        mb_free(&buffer);
        return 1;
    }

    printf("version=%u coordinate=%d identifier=%08X\n",
           (unsigned)version,
           coordinate,
           identifier);

    mb_free(&buffer);
    return 0;
}
```

## Difficult edge cases and defined boundaries

| Case | Defined behavior |
| --- | --- |
| Empty buffer | Valid with all fields zero and `data == NULL`. |
| Read exactly to the end | Succeeds and leaves `mb_isEOF` true. |
| Read past the end | Fails without moving or modifying output. |
| Seek past the end | Fails; does not grow. |
| Write at the end | Appends and may grow capacity. |
| Write beyond the end | Fails through `mb_writeAt`; use `mb_resize` for a zero-filled gap. |
| Resize growth | Newly exposed bytes are zero. |
| Resize shrink | Cursor is clamped; capacity is retained. |
| Allocation failure | Previous state remains valid and unchanged. |
| `position + count` overflow | Operation fails before pointer arithmetic or memory access. |
| Empty pattern scan | Succeeds at the current position. |
| Missing pattern | Fails and preserves position. |
| Signed 24-bit `FF FF FF` | Decodes to `-1`. |
| Out-of-range 24-bit write | Fails rather than silently truncating. |
| Source overlaps buffer | Supported when the source range is inside initialized data. |
| Exposed data pointer after growth | May be invalid; reacquire it. |

## Migration from the previous implementation

### Structure

Old:

```c
struct _memory_buffer_t
{
    uint8_t* data;
    size_t length;
    uint8_t* _pointer;
};
```

New:

```c
typedef struct memory_buffer_t
{
    uint8_t* data;
    size_t length;
    size_t capacity;
    size_t position;
} memory_buffer_t;
```

Use `mb_position(&buffer)` or `buffer.position` instead of subtracting pointers.

### `mb_initFromMemory`

Old behavior implicitly adopted the pointer and later freed it. New behavior
copies the bytes and returns `bool`.

Most old calls still compile when the result is ignored:

```c
mb_initFromMemory(&buffer, data, length);
```

New code should check the result:

```c
if (!mb_initFromMemory(&buffer, data, length))
{
    /* handle allocation failure */
}
```

Use `mb_initTakeOwnership` when transfer is intended.

### `mb_seek`

Old behavior could grow the buffer. New behavior is positioning-only.

Replace:

```c
mb_seek(&buffer, new_size);
```

with:

```c
if (mb_resize(&buffer, new_size))
    mb_seek(&buffer, new_size);
```

when growth is intended.

### 24-bit writes

Old writes silently discarded high bits. New writes reject out-of-range values.
Mask explicitly before calling an unsigned writer only when truncation is truly
the desired file-format behavior:

```c
mb_writeUInt24LE(&buffer, value & MB_UINT24_MAX);
```

## Testing

The accompanying `memory_buffer_test.c` contains 53 tests covering:

- lifecycle and ownership;
- copy and detach semantics;
- reserve, resize, clear, rewind, and shrink-to-fit;
- strict seek and skip boundaries;
- transactional search behavior;
- raw read/write and random-access operations;
- zero-length and null-pointer cases;
- internal overlapping writes;
- deterministic allocation failures;
- arithmetic-overflow rejection;
- signed and unsigned 16-, 24-, 32-, and 64-bit boundaries in both byte orders;
- the previous UBSan high-bit shift case;
- short typed reads preserving cursor and output;
- mixed-format integration;
- deterministic sequential and model-based stress tests.

Run the project commands:

```sh
./nob test memory_buffer_test
./nob ubsan memory_buffer_test
./nob asan memory_buffer_test
./nob valgrind memory_buffer_test
```

Recommended compiler-warning matrix:

```sh
-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
```

Run both GCC and Clang in CI because their warning and sanitizer diagnostics are
complementary.
