# wav.h

Single-header C library for WAV file I/O and PCM resampling.

## Overview

`wav.h` reads and writes RIFF/WAVE PCM audio files, supporting 8-bit and 16-bit samples, mono and stereo channels. It also provides a fast PCM resampler with optimised paths for common integer ratios (2x, 4x up/down for 8-bit mono) and a general fixed-point linear interpolation fallback for all other rates and formats.

## Setup

Define `MINIWAVE_IMPLEMENTATION` in **exactly one** translation unit before including the header:

```c
#define MINIWAVE_IMPLEMENTATION
#include "wav.h"
```

In all other translation units include it without the define:

```c
#include "wav.h"
```

## Custom Allocator

Override the three allocator macros before the implementation include:

```c
#define MINIWAVE_MALLOC(sz)         my_malloc(sz)
#define MINIWAVE_REALLOC(ptr, size) my_realloc(ptr, size)
#define MINIWAVE_FREE(ptr)          my_free(ptr)
#define MINIWAVE_IMPLEMENTATION
#include "wav.h"
```

## Data Structures

### `mw_wav_header`

Packed representation of the RIFF/WAVE file header (44 bytes, `#pragma pack(push,1)`):

| Field            | Type       | Description                              |
|------------------|------------|------------------------------------------|
| `riff_marker`    | `char[4]`  | `"RIFF"`                                 |
| `overall_size`   | `uint32_t` | Total file size minus 8                  |
| `wave_marker`    | `char[4]`  | `"WAVE"`                                 |
| `fmt_marker`     | `char[4]`  | `"fmt "`                                 |
| `fmt_length`     | `uint32_t` | Format chunk length (16 for PCM)         |
| `audio_format`   | `uint16_t` | `1` = PCM (uncompressed)                 |
| `channels`       | `uint16_t` | `1` = mono, `2` = stereo                 |
| `sample_rate`    | `uint32_t` | e.g. 11025, 22050, 44100                 |
| `byte_rate`      | `uint32_t` | `sample_rate × channels × (bps/8)`       |
| `block_align`    | `uint16_t` | `channels × (bps/8)`                     |
| `bits_per_sample`| `uint16_t` | 8 or 16                                  |
| `data_marker`    | `char[4]`  | `"data"`                                 |
| `data_size`      | `uint32_t` | Byte length of the raw PCM payload       |

### `mw_audio_buffer`

Decoded audio representation:

```c
typedef struct {
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t bits_per_sample;
    uint32_t data_length;   /* byte length of `data` */
    uint8_t* data;          /* heap-allocated raw PCM samples */
} mw_audio_buffer;
```

`data` is always owned by the caller. Use `mw_free_buffer` to release it.

## API Reference

### `mw_read_memory`

```c
int32_t mw_read_memory(const void* buffer, size_t buffer_size, mw_audio_buffer* out_audio);
```

Parses a WAV file from an in-memory buffer.

- **Returns** `1` on success, `0` on failure (malformed header, allocation error, truncated data).
- Scans past unknown metadata chunks (e.g. `LIST`, `INFO`) to locate the `data` chunk.
- On success, `out_audio->data` is heap-allocated; call `mw_free_buffer` when done.

### `mw_read_file`

```c
int32_t mw_read_file(const char* filename, mw_audio_buffer* out_audio);
```

Reads the entire file into a temporary buffer, then calls `mw_read_memory`.

- **Returns** `1` on success, `0` on failure (file not found, I/O error, parse error).

### `mw_write_memory`

```c
void* mw_write_memory(const mw_audio_buffer* audio, size_t* out_buffer_size);
```

Serialises `audio` into a canonical WAV file image.

- **Returns** a heap-allocated buffer containing the complete WAV file, or `NULL` on failure.
- Caller must free the returned buffer with `MINIWAVE_FREE`.
- Sets `*out_buffer_size` to the total byte count.

### `mw_write_file`

```c
int32_t mw_write_file(const char* filename, const mw_audio_buffer* audio);
```

Writes `audio` to disk as a PCM WAV file.

- **Returns** `1` on success, `0` on failure.

### `mw_resample_pcm`

```c
int32_t mw_resample_pcm(const mw_audio_buffer* src, mw_audio_buffer* dst, uint32_t target_sample_rate);
```

Resamples `src` to `target_sample_rate`, storing the result in `dst`.

- **Returns** `1` on success, `0` on failure (NULL inputs, zero rate, allocation error).
- `dst->data` is heap-allocated; call `mw_free_buffer` when done.
- `src` is not modified.

**Fast paths** (8-bit mono only):

| Condition                  | Method                        |
|----------------------------|-------------------------------|
| Same rate                  | `memcpy` (identity copy)      |
| `target = src × 2`         | `mw_upsample_2x_u8`           |
| `target = src × 4`         | `mw_upsample_4x_u8`           |
| `target = src / 2`         | `mw_downsample_2x_u8`         |
| `target = src / 4`         | `mw_downsample_4x_u8`         |

**General fallback** (all channels/bit-depths): fixed-point 16.16 linear interpolation.

### `mw_free_buffer`

```c
void mw_free_buffer(mw_audio_buffer* audio);
```

Frees `audio->data` and sets it to `NULL`. Safe to call on a zero-initialised struct.

## Error Handling

All functions return `0` / `NULL` on failure. No global state or error codes are set. Callers should check return values and treat `out_audio` as undefined after a failure.

## Supported Formats

| Feature              | Supported |
|----------------------|-----------|
| 8-bit PCM mono       | Yes       |
| 8-bit PCM stereo     | Yes       |
| 16-bit PCM mono      | Yes       |
| 16-bit PCM stereo    | Yes       |
| 24/32-bit PCM        | No        |
| IEEE float           | No        |
| Compressed formats   | No        |
| Metadata chunks      | Read past |
| Streaming write      | No        |

## Known Limitations / Proposed Extensions

1. **24/32-bit PCM support** – `mw_read_memory` and `mw_write_memory` only handle 8 and 16 bits per sample. A `bits_per_sample == 24` or `32` path in `mw_resample_pcm` and the write routines would broaden compatibility.

2. **IEEE 754 floating-point audio** (`audio_format == 3`) – Common in DAW exports; not currently decoded.

3. **Streaming write** – Large files require the entire PCM payload to reside in memory before writing. A streaming API that patches the `data_size` field on `fclose` would reduce peak RAM usage.

4. **Metadata exposure** – `LIST`/`INFO` chunks are safely skipped but not parsed. Returning a key-value map of RIFF metadata would be useful for archival tools.

5. **Channel conversion** – No built-in mono-to-stereo or stereo-to-mono mixer. Users must implement this themselves before resampling.

6. **24-bit path in fallback resampler** – The `mw_resample_pcm` general fallback only interpolates 8-bit and 16-bit samples; a 24-bit branch is absent.

## Examples

### Read and resample a WAV file

```c
#define MINIWAVE_IMPLEMENTATION
#include "wav.h"

int main(void) {
    mw_audio_buffer src = {0};
    if (!mw_read_file("input.wav", &src)) {
        fprintf(stderr, "Failed to read input.wav\n");
        return 1;
    }

    mw_audio_buffer resampled = {0};
    if (!mw_resample_pcm(&src, &resampled, 44100)) {
        fprintf(stderr, "Resampling failed\n");
        mw_free_buffer(&src);
        return 1;
    }

    mw_write_file("output_44100.wav", &resampled);
    mw_free_buffer(&src);
    mw_free_buffer(&resampled);
    return 0;
}
```

### Write a synthetic tone

```c
#define MINIWAVE_IMPLEMENTATION
#include "wav.h"
#include <stdlib.h>
#include <math.h>

int main(void) {
    const uint32_t rate = 44100;
    const uint32_t samples = rate; /* 1 second */

    mw_audio_buffer audio = {
        .channels       = 1,
        .sample_rate    = rate,
        .bits_per_sample = 16,
        .data_length    = samples * 2,
        .data           = malloc(samples * 2)
    };

    int16_t* pcm = (int16_t*)audio.data;
    for (uint32_t i = 0; i < samples; i++)
        pcm[i] = (int16_t)(32767.0 * sin(2.0 * 3.14159265 * 440.0 * i / rate));

    mw_write_file("tone_440hz.wav", &audio);
    mw_free_buffer(&audio);
    return 0;
}
```

## License

MIT License — Copyright (c) 2026 Alejandro Coto Gutiérrez.
