# FLIC reader

`flic.h` reads Autodesk Animator `FLI` and `FLC` animation files. It parses file metadata, decodes frame chunks into a palette-indexed working frame, and can expand changed pixels into a 24-bit RGB image buffer.

Define `SHL_FLIC_IMPLEMENTATION` in exactly one translation unit before including `flic.h` to compile the implementation.

```c
#define SHL_FLIC_IMPLEMENTATION
#include "flic.h"
```

## Types

| Name | Description |
| --- | --- |
| `Flic` | File reader state containing the file handle, dimensions, frame count, playback speed, frame offsets, and current frame index. |
| `FlicFrame` | Decoded working frame containing `pixels`, `rowStride`, and the current 256-color palette. |

## Constants

| Name | Description |
| --- | --- |
| `FLI_MAGIC_NUMBER` | Magic value for `.fli` files. |
| `FLC_MAGIC_NUMBER` | Magic value for `.flc` files. |
| `FLI_FRAME_MAGIC_NUMBER` | Magic value for frame headers. |
| `FLI_COLORS_SIZE` | Size in bytes of the RGB palette buffer (`256 * 3`). |
| `FLI_PXL_CHANGE` | Bit flag used in `FlicFrame.pixels` to mark pixels that changed. |
| `FLI_PXL_INDEX` | Mask used to extract the palette index from a pixel entry. |

## API

| Function | Description | Return type |
| --- | --- | --- |
| `flicOpen`(Flic* flic, const char* filename) | Opens a FLIC file, reads the header, and initializes the reader state. | `bool` |
| `flicReadFrame`(Flic* flic, FlicFrame* frame) | Decodes the next frame into `frame`, applying palette and pixel changes in place. | `bool` |
| `flicMakeImage`(Flic* flic, FlicFrame* frame, uint8_t* image) | Expands changed pixels from `frame` into an RGB image buffer. | `void` |
| `flicClose`(Flic* flic) | Closes the file handle held by the reader. | `void` |

Notes:

* `flicOpen()` defaults missing dimensions to `320x200`, matching the implementation behavior.
* `FlicFrame.pixels` must point to writable storage large enough for `rowStride * flic->height` `uint16_t` values.
* `flicReadFrame()` updates the existing frame state. Keep the same `FlicFrame` between calls when decoding a full animation.
* `flicMakeImage()` only writes pixels flagged with `FLI_PXL_CHANGE`, so callers typically preserve the previous RGB image between frames.

Example:

```c
#define SHL_FLIC_IMPLEMENTATION
#include "flic.h"

int main(void)
{
    Flic flic;
    if (!flicOpen(&flic, "intro.flc"))
        return 1;

    uint16_t* pixels = (uint16_t*)calloc(flic.width * flic.height, sizeof(uint16_t));
    uint8_t* image = (uint8_t*)calloc(flic.width * flic.height * 3, sizeof(uint8_t));
    FlicFrame frame = {0};
    frame.pixels = pixels;
    frame.rowStride = flic.width;

    if (flicReadFrame(&flic, &frame))
        flicMakeImage(&flic, &frame, image);

    free(image);
    free(pixels);
    flicClose(&flic);
    return 0;
}
```
