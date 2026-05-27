# xmi2mid.h

Single-header C library for transcoding XMI (Extended MIDI) data to Standard MIDI Files (SMF).

## Overview

`xmi2mid.h` converts the XMI format — used by Origin Systems games (Ultima VII, Wing Commander, etc.) and the Miles Sound System — into standard `.mid` files that any MIDI player can read.

XMI differs from standard MIDI in several key ways:

- **Absolute-time note durations**: Note-Off events are encoded inline with Note-On as a variable-length duration field rather than as separate events at a later time offset.
- **IFF-style chunking**: Audio data is wrapped in an IFF container; the relevant chunk is identified by the four-byte tag `EVNT`.
- **Byte-value delays**: Time advances are encoded as raw delay bytes (values < 0x80), not as standard MIDI variable-length quantities.
- **Timing scale**: XMI ticks are 3× finer than the MIDI ticks the library emits (`XMI_TIMING_SCALER = 3`).

## Setup

Define `XMI2MID_IMPLEMENTATION` in **exactly one** translation unit before including the header:

```c
#define XMI2MID_IMPLEMENTATION
#include "xmi2mid.h"
```

In all other translation units include it without the define:

```c
#include "xmi2mid.h"
```

## Custom Allocator

Override the two allocator macros before the implementation include:

```c
#define X2M_REALLOC(ptr, size) my_realloc(ptr, size)
#define X2M_FREE(ptr)          my_free(ptr)
#define XMI2MID_IMPLEMENTATION
#include "xmi2mid.h"
```

Note: the library uses only `X2M_REALLOC` (both for initial allocation and growth) and `X2M_FREE`.

## API Reference

### `x2m_transcode`

```c
uint8_t* x2m_transcode(uint8_t* xmiData, size_t xmiLength, size_t* midLength);
```

Transcodes raw XMI data into a complete Standard MIDI File (SMF format 0, one track).

**Parameters**

| Parameter   | Direction | Description                                        |
|-------------|-----------|----------------------------------------------------|
| `xmiData`   | in        | Pointer to the raw XMI file bytes                  |
| `xmiLength` | in        | Byte length of the XMI input buffer                |
| `midLength` | out       | Receives the byte length of the returned MIDI data; may be `NULL` |

**Returns** a heap-allocated buffer containing the complete MIDI file, or `NULL` on failure.

The caller is responsible for freeing the returned buffer with `X2M_FREE()`.

**Failure conditions**

- `xmiData` is `NULL` or `xmiLength` is 0
- The IFF `EVNT` chunk marker is not found
- The input is truncated mid-event
- An allocation failure occurs

## Transcoding Pipeline

The library processes XMI in seven steps:

1. **Pre-allocate** output buffer (`xmiLength + 256` bytes, grown as needed).
2. **Scan** for the `EVNT` IFF chunk tag; skip the 8-byte tag+size header.
3. **Parse pass**: read events sequentially, accumulating delay bytes into the current time, appending typed `midi_token_t` entries to an internal list. Note-On events immediately inject a paired synthetic Note-Off token at `time + duration × XMI_TIMING_SCALER`.
4. **Write MIDI header** (`MThd`, format 0, 1 track, ticks-per-beat).
5. **Sort** all tokens by time using an unstable in-place quicksort.
6. **Write pass**: emit tokens in sorted order with MIDI variable-length deltas and running-status optimisation.
7. **Patch** the `MTrk` chunk length field with the actual byte count.

## Timing and Ticks-Per-Beat

The first tempo meta-event in the XMI stream sets the output ticks-per-beat:

```
ticks_per_beat = (tempo_µs × XMI_TIMING_SCALER²) / 25000
```

For the standard 500 000 µs/beat (120 BPM) default:

```
(500000 × 3 × 3) / 25000 = 180 ticks/beat
```

Subsequent tempo changes are discarded (only the first is used).

## Running-Status Optimisation

The write pass applies the standard MIDI running-status rule: the status byte is omitted when two consecutive events share the same type and channel. Note-Off events are synthesised as Note-On with velocity `0x00` and share the preceding Note-On status byte, so no explicit `0x80` Note-Off status byte appears in the output.

## Sort Stability Warning

The internal quicksort is **not stable**. When two events share the same timestamp the relative order in the output is undefined. Build XMI input so that no two events have equal times if byte-exact output is required (e.g. add a 1-tick delay before EOT).

## Output Format

The returned buffer is a complete, self-contained SMF format-0 file:

| Offset | Size | Content                                 |
|--------|------|-----------------------------------------|
| 0      | 4    | `MThd`                                  |
| 4      | 4    | Chunk length = `0x00000006` (BE)        |
| 8      | 2    | Format = `0x0000` (single-track) (BE)  |
| 10     | 2    | Number of tracks = `0x0001` (BE)       |
| 12     | 2    | Ticks per beat (BE)                     |
| 14     | 4    | `MTrk`                                  |
| 18     | 4    | Track byte length (BE, patched post-emit)|
| 22     | n    | MIDI event stream                       |

## Error Handling

`x2m_transcode` returns `NULL` on any failure. No global state is modified. The caller should check the return value before using the buffer.

## Limitations / Proposed Extensions

1. **Single-track output only** — Multi-track XMI files (XMIDI with multiple `EVNT` chunks) produce only the first track. A loop over `EVNT` chunks with SMF format-1 output would support full multi-track conversion.

2. **First tempo only** — Only the first `0xFF 0x51` meta-event is applied; subsequent tempo changes are dropped. Full tempo-map support would require re-emitting them at their correct MIDI times.

3. **Unstable sort** — Equal-time events are reordered arbitrarily. A stable sort (merge sort or insertion sort for small counts) would give deterministic output from any input.

4. **No SysEx support** — System-exclusive events in the XMI stream are not handled and may corrupt the output.

5. **`xmiData` is non-const** — The function signature takes `uint8_t*` rather than `const uint8_t*`; the implementation does not modify the input, but the non-const signature prevents passing read-only buffers without a cast.

6. **Single allocation growth** — The output buffer is doubled on each realloc. For very large XMI files this can transiently use up to 2× the output size. A more precise pre-sizing pass would reduce peak memory.

## Examples

### Transcode an XMI file to MIDI

```c
#include <stdio.h>
#include <stdlib.h>

#define XMI2MID_IMPLEMENTATION
#include "xmi2mid.h"

int main(void) {
    /* Read XMI file */
    FILE* f = fopen("music.xmi", "rb");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    long xmi_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* xmi = (uint8_t*)malloc(xmi_len);
    fread(xmi, 1, xmi_len, f);
    fclose(f);

    /* Transcode */
    size_t mid_len = 0;
    uint8_t* mid = x2m_transcode(xmi, xmi_len, &mid_len);
    free(xmi);
    if (!mid) { fprintf(stderr, "Transcoding failed\n"); return 1; }

    /* Write MIDI file */
    f = fopen("music.mid", "wb");
    fwrite(mid, 1, mid_len, f);
    fclose(f);
    X2M_FREE(mid);
    return 0;
}
```

### Use a custom allocator

```c
#define X2M_REALLOC(ptr, size) my_pool_realloc(ptr, size)
#define X2M_FREE(ptr)          my_pool_free(ptr)
#define XMI2MID_IMPLEMENTATION
#include "xmi2mid.h"
```

## License

MIT License — Copyright (c) 2026 Alejandro Coto Gutiérrez.
