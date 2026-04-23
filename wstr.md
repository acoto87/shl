# Wstr string library

`wstr.h` provides two complementary string types:

* `StringView`: a non-owning, read-only view into existing character data.
* `String`: a heap-allocated, null-terminated, mutable string.

Define `SHL_WSTR_IMPLEMENTATION` in exactly one translation unit before including `wstr.h` to compile the implementation.

```c
#define SHL_WSTR_IMPLEMENTATION
#include "wstr.h"
```

You can override the allocator used by `String` before the implementation include:

```c
#define WSTR_MALLOC(sz) my_malloc(sz)
#define WSTR_REALLOC(p, sz) my_realloc((p), (sz))
#define WSTR_FREE(p) my_free(p)
#define SHL_WSTR_IMPLEMENTATION
#include "wstr.h"
```

## Helpers and types

| Name | Description |
| --- | --- |
| `WSV_NPOS` | Sentinel returned by search functions when a match is not found. |
| `WSV_LITERAL(text)` | Builds a compile-time `StringView` from a string literal. |
| `StringView` | Non-owning view with `data` and `length`. |
| `String` | Owning mutable string with `data`, `length`, and `capacity`. |

## StringView API

The `wsv_` functions work on slices of existing memory and never allocate unless explicitly converting to `String`.

| Function | Description | Return type |
| --- | --- | --- |
| `wsv_empty`(void) | Returns an empty string view. | `StringView` |
| `wsv_fromCString`(const char* text) | Wraps a null-terminated C string. `NULL` becomes an empty view. | `StringView` |
| `wsv_fromParts`(const char* text, size_t length) | Wraps a pointer plus explicit length. | `StringView` |
| `wsv_fromRange`(const char* begin, const char* end) | Wraps the half-open range `[begin, end)`. Invalid ranges become empty. | `StringView` |
| `wsv_fromString`(const String* string) | Creates a view over a `String`. | `StringView` |
| `wsv_isEmpty`(StringView view) | Returns whether the view length is zero. | `bool` |
| `wsv_data`(StringView view) | Returns `view.data`, or `""` for empty/null views. | `const char*` |
| `wsv_length`(StringView view) | Returns the view length. | `size_t` |
| `wsv_slice`(StringView view, size_t index, size_t length) | Returns a clamped subrange. | `StringView` |
| `wsv_subview`(StringView view, size_t index) | Returns the suffix starting at `index`. | `StringView` |
| `wsv_trimLeft`(StringView view) | Trims ASCII whitespace on the left. | `StringView` |
| `wsv_trimRight`(StringView view) | Trims ASCII whitespace on the right. | `StringView` |
| `wsv_trim`(StringView view) | Trims ASCII whitespace on both ends. | `StringView` |
| `wsv_equals`(StringView left, StringView right) | Compares two views for exact equality. | `bool` |
| `wsv_equalsIgnoreCase`(StringView left, StringView right) | Compares two views case-insensitively using ASCII lowercase conversion. | `bool` |
| `wsv_startsWith`(StringView view, StringView prefix) | Returns whether `view` starts with `prefix`. | `bool` |
| `wsv_startsWithIgnoreCase`(StringView view, StringView prefix) | Case-insensitive prefix check. | `bool` |
| `wsv_findChar`(StringView view, char c) | Finds the first occurrence of `c`. | `size_t` |
| `wsv_find`(StringView view, StringView needle) | Finds the first occurrence of `needle`. | `size_t` |
| `wsv_findAny`(StringView view, StringView chars) | Finds the first character contained in `chars`. | `size_t` |
| `wsv_skipChars`(StringView view, StringView chars) | Skips a leading run of characters present in `chars`. | `StringView` |
| `wsv_takeUntilAny`(StringView view, StringView chars) | Returns the prefix up to the first character present in `chars`. | `StringView` |
| `wsv_splitOnce`(StringView view, StringView separator, StringView* left, StringView* right) | Splits at the first separator. Returns `true` when a separator was found. | `bool` |
| `wsv_chopByDelimiter`(StringView* remaining, char delimiter) | Consumes and returns the next delimited token. | `StringView` |
| `wsv_nextToken`(StringView* remaining, StringView separators, StringView* token) | Iterates tokens separated by any character in `separators`. | `bool` |
| `wsv_hashFNV32`(StringView view) | Computes a 32-bit FNV-1 hash. | `uint32_t` |
| `wsv_parseS32`(StringView view) | Parses a signed 32-bit integer, returning `0` on failure. | `int32_t` |
| `wsv_tryParseS32`(StringView view, int32_t* value) | Parses a signed 32-bit integer with success reporting. | `bool` |
| `wsv_parseS64`(StringView view) | Parses a signed 64-bit integer, returning `0` on failure. | `int64_t` |
| `wsv_tryParseS64`(StringView view, int64_t* value) | Parses a signed 64-bit integer with success reporting. | `bool` |
| `wsv_copyToBuffer`(StringView view, char* buffer, size_t capacity) | Copies the view and appends a null terminator if it fits. | `bool` |
| `wsv_toString`(StringView view) | Allocates an owning `String` copy. | `String` |

Integer parsing accepts optional leading whitespace, an optional sign, decimal numbers, `0x`/`0X` hexadecimal, and leading-`0` octal.

## String API

The `wstr_` functions own their storage and keep `data` null-terminated.

| Function | Description | Return type |
| --- | --- | --- |
| `wstr_make`(void) | Returns an empty string. | `String` |
| `wstr_withCapacity`(size_t capacity) | Creates an empty string with reserved capacity. | `String` |
| `wstr_fromCString`(const char* text) | Creates a string from a C string. | `String` |
| `wstr_fromCStringFormat`(const char* textFormat, ...) | Creates a formatted string. | `String` |
| `wstr_fromCStringFormatv`(const char* textFormat, va_list args) | `va_list` variant of formatted construction. | `String` |
| `wstr_fromView`(StringView view) | Copies a view into a new string. | `String` |
| `wstr_concat`(StringView left, StringView right) | Returns a new string containing `left` followed by `right`. | `String` |
| `wstr_adopt`(char* buffer, size_t length, size_t capacity) | Adopts an existing mutable buffer and ensures it is null-terminated at `length`. | `String` |
| `wstr_copy`(const String* string) | Returns a fully independent heap-allocated copy of `string`. `NULL` or empty input returns an empty string. | `String` |
| `wstr_freePtr`(String* string) | Frees the owned storage and resets the struct. | `void` |
| `wstr_free`(String string) | Frees the owned storage in a by-value string. | `void` |
| `wstr_clear`(String* string) | Resets the string to length `0` without releasing capacity. | `void` |
| `wstr_view`(const String* string) | Returns a `StringView` over the string contents. | `StringView` |
| `wstr_cstr`(const String* string) | Returns a null-terminated pointer, or `""` for null/empty strings. | `const char*` |
| `wstr_isEmpty`(const String* string) | Returns whether the string is null or empty. | `bool` |
| `wstr_reserve`(String* string, size_t capacity) | Ensures at least `capacity` bytes are available. | `bool` |
| `wstr_resize`(String* string, size_t length) | Changes the logical length and keeps the data null-terminated. New bytes are zero-filled. | `bool` |
| `wstr_assign`(String* string, StringView view) | Replaces the string contents with `view`. | `bool` |
| `wstr_assignCString`(String* string, const char* text) | Assigns from a C string. | `bool` |
| `wstr_assignCStringFormat`(String* string, const char* textFormat, ...) | Replaces the string with formatted text. | `bool` |
| `wstr_assignCStringFormatv`(String* string, const char* textFormat, va_list args) | `va_list` variant of formatted assignment. | `bool` |
| `wstr_append`(String* string, StringView view) | Appends a view. | `bool` |
| `wstr_appendCString`(String* string, const char* text) | Appends a C string. | `bool` |
| `wstr_appendCStringFormat`(String* string, const char* textFormat, ...) | Appends formatted text. | `bool` |
| `wstr_appendCStringFormatv`(String* string, const char* textFormat, va_list args) | `va_list` variant of formatted append. | `bool` |
| `wstr_appendChar`(String* string, char c) | Appends one character. | `bool` |
| `wstr_insert`(String* string, size_t index, StringView view) | Inserts text at `index`. | `bool` |
| `wstr_removeRange`(String* string, size_t index, size_t length) | Removes a clamped range. | `bool` |
| `wstr_setFormat`(String* string, const char* fmt, ...) | Replaces the contents with formatted text. | `bool` |
| `wstr_setFormatv`(String* string, const char* fmt, va_list args) | `va_list` variant of formatted replacement. | `bool` |
| `wstr_appendFormat`(String* string, const char* fmt, ...) | Appends formatted text. | `bool` |
| `wstr_appendFormatv`(String* string, const char* fmt, va_list args) | `va_list` variant of formatted append. | `bool` |

The mutating string functions handle aliasing safely, so views derived from the same string can be passed back into `wstr_assign`, `wstr_append`, and `wstr_insert`.

Example:

```c
#define SHL_WSTR_IMPLEMENTATION
#include "wstr.h"

int main(void)
{
    String path = wstr_fromCString("  assets/sound.wav  ");
    StringView trimmed = wsv_trim(wstr_view(&path));

    String fullPath = wstr_concat(WSV_LITERAL("data/"), trimmed);
    wstr_appendCString(&fullPath, ".bak");

    wstr_free(path);
    wstr_free(fullPath);
    return 0;
}
```
