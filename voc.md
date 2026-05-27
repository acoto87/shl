# voc.h

Single-header C library for Creative Voice File (VOC) I/O and PCM resampling.

## Overview

`voc.h` reads and writes Creative Voice File (`.voc`) audio, the format used by Sound Blaster cards and many classic DOS game engines. It supports block types 0x01 (legacy 8-bit), 0x08 (extended attribute), and 0x09 (modern unified block), covering mono/stereo PCM at 8 and 16 bits per sample. It also provides the same fast PCM resampler as `wav.h` — optimised integer-ratio paths (2x, 4x up/down for 8-bit mono) plus a general fixed-point linear interpolation fallback.

## Setup

Define `MINIVOC_IMPLEMENTATION` in **exactly one** translation unit before including the header:

```c
#define MINIVOC_IMPLEMENTATION
#include "voc.h"
```

In all other translation units include it without the define:

```c
#include "voc.h"
```

## Custom Allocator

Override the three allocator macros before the implementation include:

```c
#define MINIVOC_MALLOC(sz)         my_malloc(sz)
#define MINIVOC_REALLOC(ptr, size) my_realloc(ptr, size)
#define MINIVOC_FREE(ptr)          my_free(ptr)
#define MINIVOC_IMPLEMENTATION
#include "voc.h"
```

## Data Structures

### `mv_audio_buffer`

Decoded audio representation:

```c
typedef struct {
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t bits_per_sample;
    uint32_t data_length;   /* byte length of `data` */
    uint8_t* data;          /* heap-allocated raw PCM samples */
} mv_audio_buffer;
```

`data` is always owned by the caller. Use `mv_free_buffer` to release it.

## VOC File Structure

A VOC file begins with a 26-byte fixed header followed by a sequence of typed blocks:

| Offset | Size | Value            | Description                         |
|--------|------|------------------|-------------------------------------|
| 0      | 20   | `"Creative Voice File\x1A"` | Magic identifier         |
| 20     | 2    | LE uint16        | Offset to first block (always 26)   |
| 22     | 2    | LE uint16        | Version (`0x0114` = v1.20)          |
| 24     | 2    | LE uint16        | Checksum `(uint16_t)(~version + 0x1234)` |

Blocks immediately follow at the data offset. Each block has a 1-byte type, a 3-byte LE length, and a type-specific payload. Block type `0x00` signals end of file.

### Supported Block Types

| Type   | Name                   | Description                                             |
|--------|------------------------|---------------------------------------------------------|
| `0x00` | Terminator             | End of VOC data                                         |
| `0x01` | Sound Data             | 8-bit mono PCM; requires uncompressed (pack = 0x00)    |
| `0x08` | Extended Attributes    | Sets rate/stereo for the following `0x01` block        |
| `0x09` | Sound Data (New)       | Direct rate, bits, channels, format in one block       |

Unknown block types are silently skipped, allowing forward compatibility.

## API Reference

### `mv_read_memory`

```c
int32_t mv_read_memory(const void* buffer, size_t buffer_size, mv_audio_buffer* out_audio);
```

Parses a VOC file from an in-memory buffer.

- **Returns** `1` on success, `0` on failure (bad magic, version mismatch, unsupported codec, allocation error).
- Validates the version checksum (`(uint16_t)(~version + 0x1234) == checksum`).
- Concatenates PCM data from all audio blocks in order.
- On success, `out_audio->data` is heap-allocated; call `mv_free_buffer` when done.

### `mv_read_file`

```c
int32_t mv_read_file(const char* filename, mv_audio_buffer* out_audio);
```

Reads the entire file into a temporary buffer, then calls `mv_read_memory`.

- **Returns** `1` on success, `0` on failure (file not found, I/O error, parse error).

### `mv_write_memory`

```c
void* mv_write_memory(const mv_audio_buffer* audio, size_t* out_buffer_size);
```

Serialises `audio` into a canonical VOC file image using a single block type `0x09`.

- **Returns** a heap-allocated buffer containing the complete VOC file, or `NULL` on failure.
- Caller must free the returned buffer with `MINIVOC_FREE`.
- Sets `*out_buffer_size` to the total byte count.
- Output layout: 26-byte header + 1-byte type + 3-byte length + 12-byte block-9 header + PCM data + 1-byte terminator.

### `mv_write_file`

```c
int32_t mv_write_file(const char* filename, const mv_audio_buffer* audio);
```

Writes `audio` to disk as a VOC file.

- **Returns** `1` on success, `0` on failure.

### `mv_resample_pcm`

```c
int32_t mv_resample_pcm(const mv_audio_buffer* src, mv_audio_buffer* dst, uint32_t target_sample_rate);
```

Resamples `src` to `target_sample_rate`, storing the result in `dst`.

- **Returns** `1` on success, `0` on failure (NULL inputs, zero rate, allocation error).
- `dst->data` is heap-allocated; call `mv_free_buffer` when done.
- `src` is not modified.

**Fast paths** (8-bit mono only):

| Condition                  | Method                        |
|----------------------------|-------------------------------|
| Same rate                  | `memcpy` (identity copy)      |
| `target = src × 2`         | `mv_upsample_2x_u8`           |
| `target = src × 4`         | `mv_upsample_4x_u8`           |
| `target = src / 2`         | `mv_downsample_2x_u8`         |
| `target = src / 4`         | `mv_downsample_4x_u8`         |

**General fallback** (all channels/bit-depths): fixed-point 16.16 linear interpolation.

### `mv_free_buffer`

```c
void mv_free_buffer(mv_audio_buffer* audio);
```

Frees `audio->data` and sets it to `NULL`. Safe to call on a zero-initialised struct.

## Error Handling

All functions return `0` / `NULL` on failure. No global state or error codes are set. Callers should check return values and treat `out_audio` as undefined after a failure.

## Supported Formats

| Feature                    | Supported         |
|----------------------------|-------------------|
| 8-bit PCM mono             | Yes               |
| 8-bit PCM stereo           | Yes (block 0x09)  |
| 16-bit PCM mono            | Yes (block 0x09)  |
| 16-bit PCM stereo          | Yes (block 0x09)  |
| Block 0x01 (legacy)        | Yes (mono 8-bit)  |
| Block 0x08 (ext. attr.)    | Yes               |
| Block 0x09 (unified)       | Yes               |
| ADPCM compression          | No                |
| Multiple concatenated blocks | Yes             |
| Unknown block types        | Skipped           |

## Known Limitations / Proposed Extensions

1. **ADPCM support** – Block type `0x01` with `compression != 0x00` is rejected. Decoding 2-bit and 4-bit ADPCM would restore compatibility with some older game files.

2. **16-bit block 0x01** – The legacy block only carries 8-bit mono. Block 0x09 must be used for 16-bit data; old files using other block types for 16-bit PCM are not handled.

3. **Loop blocks** – Block types `0x06` (repeat start) and `0x07` (repeat end) are silently skipped. A loop-expansion option would allow correct playback of looping sound effects.

4. **Silence blocks** – Block type `0x03` (silence) is skipped rather than expanded into zero-valued PCM. This causes incorrect duration in files that rely on silence blocks.

5. **Streaming read** – Large VOC files must be fully loaded into memory before parsing. A block-by-block streaming API would reduce peak RAM usage.

6. **Channel conversion** – No built-in mono-to-stereo or stereo-to-mono converter. Users must handle this before resampling.

## Examples

### Decode and resample a VOC file

```c
#define MINIVOC_IMPLEMENTATION
#include "voc.h"

int main(void) {
    mv_audio_buffer src = {0};
    if (!mv_read_file("sfx.voc", &src)) {
        fprintf(stderr, "Failed to read sfx.voc\n");
        return 1;
    }

    mv_audio_buffer resampled = {0};
    if (!mv_resample_pcm(&src, &resampled, 44100)) {
        fprintf(stderr, "Resampling failed\n");
        mv_free_buffer(&src);
        return 1;
    }

    mv_write_file("sfx_44100.voc", &resampled);
    mv_free_buffer(&src);
    mv_free_buffer(&resampled);
    return 0;
}
```

### Convert VOC to WAV

```c
#define MINIWAVE_IMPLEMENTATION
#include "wav.h"
#define MINIVOC_IMPLEMENTATION
#include "voc.h"

int main(void) {
    mv_audio_buffer voc = {0};
    if (!mv_read_file("sfx.voc", &voc)) return 1;

    /* mv_audio_buffer and mw_audio_buffer have identical field layout */
    mw_audio_buffer wav;
    wav.channels        = voc.channels;
    wav.sample_rate     = voc.sample_rate;
    wav.bits_per_sample = voc.bits_per_sample;
    wav.data_length     = voc.data_length;
    wav.data            = voc.data; /* borrow pointer — do not double-free */

    mw_write_file("sfx.wav", &wav);
    mv_free_buffer(&voc);
    return 0;
}
```

## License

MIT License — Copyright (c) 2026 Alejandro Coto Gutiérrez.
