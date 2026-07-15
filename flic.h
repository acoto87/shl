/*
    flic.h - acoto87 (acoto87@gmail.com)

    MIT License

    Copyright (c) 2018 Alejandro Coto Gutiérrez

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    Single-header reader for Autodesk Animator FLI/FLC animation files.
    This implementation is a C port of David Capello's Aseprite FLIC library:
    https://github.com/aseprite/flic

    USAGE
    Include this header in all translation units that need the declarations.
    Define SHL_FLIC_IMPLEMENTATION in exactly one translation unit before the
    include to compile the implementation:

        #define SHL_FLIC_IMPLEMENTATION
        #include "flic.h"

    Include the header without that define everywhere else:

        #include "flic.h"

    CUSTOMISATION
    The reader is self-contained and does not expose allocator or I/O hooks.
    If you need different file handling or memory behavior, adjust the
    implementation section directly.

    NOTES
    flic_open initializes a Flic from a file on disk, flic_readFrame decodes the
    next frame into caller-provided buffers, and flic_close releases file-backed
    resources when you are done.
*/

#ifndef SHL_FLIC_H
#define SHL_FLIC_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLI_MAGIC_NUMBER        0xAF11
#define FLC_MAGIC_NUMBER        0xAF12
#define FLI_FRAME_MAGIC_NUMBER  0xF1FA
#define FLI_COLOR_256_CHUNK     4
#define FLI_DELTA_CHUNK         7
#define FLI_COLOR_64_CHUNK      11
#define FLI_LC_CHUNK            12
#define FLI_BLACK_CHUNK         13
#define FLI_BRUN_CHUNK          15
#define FLI_COPY_CHUNK          16
#define FLI_COLORS_SIZE         768 // 256 * 3
#define FLI_PXL_CHANGE          0x100
#define FLI_PXL_INDEX           0xFF

typedef struct {
    uint16_t frames;
    uint16_t width;
    uint16_t height;
    uint16_t speed;
    uint32_t oframe1;
    uint32_t oframe2;

    FILE* file;
    uint32_t currentFrame;
    uint16_t lastFrameDelay;
} Flic;

typedef struct {
    uint16_t* pixels;
    uint32_t rowStride;
    uint8_t colors[FLI_COLORS_SIZE];
} FlicFrame;

bool flic_open(Flic* flic, const char* filename);
bool flic_readFrame(Flic* flic, FlicFrame* frame);
void flic_makeImage(Flic* flic, FlicFrame* frame, uint8_t* image);
void flic_close(Flic* flic);
void flic_rewind(Flic* flic);

#ifdef __cplusplus
}
#endif

#ifdef SHL_FLIC_IMPLEMENTATION

#include <string.h>

static inline uint8_t flic__read8(FILE* file)
{
    return fgetc(file);
}

static inline uint16_t flic__read16(FILE* file)
{
    int c1 = fgetc(file);
    if (c1 == EOF) return 0;
    int c2 = fgetc(file);
    if (c2 == EOF) return 0;

    return (uint16_t)((unsigned char)c2 << 8 | (unsigned char)c1);
}

static inline uint32_t flic__read32(FILE* file)
{
    int c1 = fgetc(file);
    if (c1 == EOF) return 0;
    int c2 = fgetc(file);
    if (c2 == EOF) return 0;
    int c3 = fgetc(file);
    if (c3 == EOF) return 0;
    int c4 = fgetc(file);
    if (c4 == EOF) return 0;

    return ((uint32_t)(unsigned char)c4 << 24) | ((uint32_t)(unsigned char)c3 << 16) | ((uint32_t)(unsigned char)c2 << 8) | (uint32_t)(unsigned char)c1;
}

static inline long flic__tell(FILE* file)
{
    return ftell(file);
}

static inline void flic__seek(FILE* file, long pos)
{
    fseek(file, pos, SEEK_SET);
}

static void flic__readBlackChunk(Flic* flic, FlicFrame* frame)
{
    uint32_t totalPixels = (uint32_t)frame->rowStride * flic->height;
    for (uint32_t i = 0; i < totalPixels; ++i)
    {
        frame->pixels[i] = FLI_PXL_CHANGE;
    }
}

static void flic__readCopyChunk(Flic* flic, FlicFrame* frame)
{
    for (int32_t y = 0; y < flic->height; ++y)
    {
        uint16_t* row = frame->pixels + frame->rowStride * y;
        for (int32_t x = 0; x < flic->width; ++x)
            row[x] = flic__read8(flic->file) | FLI_PXL_CHANGE;
    }
}

static void flic__readColorChunk(Flic* flic, FlicFrame* frame, bool is64ColorMap)
{
    uint16_t npackets = flic__read16(flic->file);

    uint8_t i = 0;
    while (npackets--)
    {
        i += flic__read8(flic->file); // Colors to skip

        uint16_t colors = (uint16_t)flic__read8(flic->file);
        if (colors == 0)
            colors = FLI_COLORS_SIZE / 3;

        for (int32_t j = 0; j < colors; ++j)
        {
            uint8_t r = flic__read8(flic->file);
            uint8_t g = flic__read8(flic->file);
            uint8_t b = flic__read8(flic->file);

            if ((int)i + j >= 256)
                break;

            if (is64ColorMap)
            {
                r = (uint8_t)(255 * ((float)r / 63));
                g = (uint8_t)(255 * ((float)g / 63));
                b = (uint8_t)(255 * ((float)b / 63));
            }

            frame->colors[(i + j) * 3 + 0] = r;
            frame->colors[(i + j) * 3 + 1] = g;
            frame->colors[(i + j) * 3 + 2] = b;
        }
    }
}

static void flic__readBrunChunk(Flic* flic, FlicFrame* frame)
{
    for (int32_t y = 0; y < flic->height; ++y)
    {
        uint16_t* row = frame->pixels + frame->rowStride * y;

        int32_t x = 0;
        flic__read8(flic->file); // Ignore number of packets (we read until x == width)

        while (x < flic->width)
        {
            int8_t count = (int8_t)flic__read8(flic->file);
            if (count >= 0)
            {
                uint8_t color = flic__read8(flic->file);
                while (count-- && x < flic->width)
                    row[x++] = color | FLI_PXL_CHANGE;
            }
            else
            {
                count = -count;
                while (count-- && x < flic->width)
                    row[x++] = flic__read8(flic->file) | FLI_PXL_CHANGE;
            }
        }
    }
}

static void flic__readLcChunk(Flic* flic, FlicFrame* frame)
{
    uint16_t skipLines = flic__read16(flic->file);
    uint16_t nlines = flic__read16(flic->file);

    for (int32_t y = skipLines; y < skipLines + nlines; ++y)
    {
        uint16_t* row = NULL;
        if (y >= 0 && y < flic->height)
        {
            row = frame->pixels + frame->rowStride * y;
        }

        int32_t x = 0;
        uint8_t npackets = flic__read8(flic->file);
        for (uint8_t p = 0; p < npackets; ++p)
        {
            uint8_t skip = flic__read8(flic->file);
            x += skip;

            int8_t count = (int8_t)flic__read8(flic->file);
            if (count >= 0)
            {
                for (int32_t i = 0; i < count; ++i)
                {
                    uint8_t color = flic__read8(flic->file);
                    if (row && x < flic->width)
                    {
                        row[x] = color | FLI_PXL_CHANGE;
                    }
                    x++;
                }
            }
            else
            {
                count = -count;
                uint8_t color = flic__read8(flic->file);
                for (int32_t i = 0; i < count; ++i)
                {
                    if (row && x < flic->width)
                    {
                        row[x] = color | FLI_PXL_CHANGE;
                    }
                    x++;
                }
            }
        }
    }
}

static void flic__readDeltaChunk(Flic* flic, FlicFrame* frame)
{
    uint16_t nlines = flic__read16(flic->file);
    int32_t y = 0;

    while (nlines--)
    {
        int16_t word = (int16_t)flic__read16(flic->file);
        while (word < 0)
        {
            if (word & 0x4000) // Has bit 14 (0x4000)
            {
                y += -word; // Skip lines
            }
            else // Only last pixel has changed
            {
                if (y >= 0 && y < flic->height)
                {
                    uint16_t* row = frame->pixels + frame->rowStride * y;
                    if (flic->width > 0)
                        row[flic->width - 1] = (word & 0xff) | FLI_PXL_CHANGE;
                }

                ++y;

                // The outer while(nlines--) already consumed one count for this
                // row; consume a second one for the last-pixel word so the loop
                // stays synchronised with the encoded line count.
                if (nlines == 0)
                    return;
                nlines--;
            }

            word = (int16_t)flic__read16(flic->file);
        }

        uint16_t npackets = (uint16_t)word;
        int32_t x = 0;

        uint16_t* row = NULL;
        if (y >= 0 && y < flic->height)
        {
            row = frame->pixels + frame->rowStride * y;
        }

        for (uint16_t p = 0; p < npackets; ++p)
        {
            x += flic__read8(flic->file); // Skip pixels

            int8_t count = (int8_t)flic__read8(flic->file); // Number of words

            if (count >= 0)
            {
                for (int32_t i = 0; i < count; ++i)
                {
                    uint8_t color1 = flic__read8(flic->file);
                    uint8_t color2 = flic__read8(flic->file);

                    if (row && x < flic->width)
                    {
                        row[x] = color1 | FLI_PXL_CHANGE;
                    }
                    x++;

                    if (row && x < flic->width)
                    {
                        row[x] = color2 | FLI_PXL_CHANGE;
                    }
                    x++;
                }
            }
            else
            {
                count = -count;

                uint8_t color1 = flic__read8(flic->file);
                uint8_t color2 = flic__read8(flic->file);

                for (int32_t i = 0; i < count; ++i)
                {
                    if (row && x < flic->width)
                    {
                        row[x] = color1 | FLI_PXL_CHANGE;
                    }
                    x++;

                    if (row && x < flic->width)
                    {
                        row[x] = color2 | FLI_PXL_CHANGE;
                    }
                    x++;
                }
            }
        }

        ++y;
    }
}

static void flic__readChunk(Flic* flic, FlicFrame* frame)
{
    long chunkStartPos = flic__tell(flic->file);
    uint32_t chunkSize = flic__read32(flic->file);
    uint16_t type = flic__read16(flic->file);

    switch (type) {
        case FLI_COLOR_256_CHUNK:
            flic__readColorChunk(flic, frame, false);
            break;
        case FLI_DELTA_CHUNK:
            flic__readDeltaChunk(flic, frame);
            break;
        case FLI_COLOR_64_CHUNK:
            flic__readColorChunk(flic, frame, true);
            break;
        case FLI_LC_CHUNK:
            flic__readLcChunk(flic, frame);
            break;
        case FLI_BLACK_CHUNK:
            flic__readBlackChunk(flic, frame);
            break;
        case FLI_BRUN_CHUNK:
            flic__readBrunChunk(flic, frame);
            break;
        case FLI_COPY_CHUNK:
            flic__readCopyChunk(flic, frame);
            break;
        default:
            // Ignore all other kind of chunks
            break;
    }

    flic__seek(flic->file, chunkStartPos + chunkSize);
}

bool flic_open(Flic* flic, const char* filename)
{
    if (!flic || !filename)
        return false;

    memset(flic, 0, sizeof(Flic));

    flic->file = fopen(filename, "rb");
    if (!flic->file)
        return false;

    flic__read32(flic->file); // file size

    uint16_t magic = flic__read16(flic->file);
    if (magic != FLI_MAGIC_NUMBER && magic != FLC_MAGIC_NUMBER)
    {
        fclose(flic->file);
        flic->file = NULL;
        return false;
    }

    flic->frames = flic__read16(flic->file);
    flic->width  = flic__read16(flic->file);
    flic->height = flic__read16(flic->file);

    flic__read16(flic->file); // Color depth (it is interpreted as 8bpp anyway)
    flic__read16(flic->file); // Skip flags

    flic->speed = flic__read32(flic->file);

    if (magic == FLI_MAGIC_NUMBER)
    {
        if (flic->speed == 0)
            flic->speed = 70;
        else
            flic->speed = 1000 * flic->speed / 70;

        flic->oframe1 = 128;
    }

    if (magic == FLC_MAGIC_NUMBER)
    {
        // Offset to the first and second frame header values
        flic__seek(flic->file, 80);

        flic->oframe1 = flic__read32(flic->file);
        flic->oframe2 = flic__read32(flic->file);
    }

    if (flic->width == 0) flic->width = 320;
    if (flic->height == 0) flic->height = 200;

    // Skip padding
    flic__seek(flic->file, 128);
    return true;
}

void flic_close(Flic* flic)
{
    if (!flic)
        return;

    if (flic->file)
    {
        fclose(flic->file);
        flic->file = NULL;
    }
}

bool flic_readFrame(Flic* flic, FlicFrame* frame)
{
    if (!flic || !frame || !frame->pixels || !flic->file)
        return false;

    switch (flic->currentFrame)
    {
        case 0:
        {
            if (flic->oframe1)
                flic__seek(flic->file, flic->oframe1);

            break;
        }

        case 1:
        {
            if (flic->oframe2)
                flic__seek(flic->file, flic->oframe2);

            break;
        }
    }

    long frameStartPos = flic__tell(flic->file);
    uint32_t frameSize = flic__read32(flic->file);

    uint16_t magic = flic__read16(flic->file);
    if (magic != FLI_FRAME_MAGIC_NUMBER)
        return false;

    uint16_t chunks = flic__read16(flic->file);
    uint16_t delay = flic__read16(flic->file);
    flic->lastFrameDelay = delay;

    for (int32_t i = 0; i < 6; ++i)       // remaining 6 bytes of the 8-byte padding/overrides
        flic__read8(flic->file);

    for (int32_t i = 0; i < chunks; ++i)
        flic__readChunk(flic, frame);

    flic__seek(flic->file, frameStartPos + frameSize);

    if (flic->currentFrame == 0 && flic->oframe2 == 0)
    {
        flic->oframe2 = frameStartPos + frameSize;
    }

    flic->currentFrame++;
    return true;
}

void flic_makeImage(Flic* flic, FlicFrame* frame, uint8_t* image)
{
    if (!flic || !frame || !frame->pixels || !image)
        return;

    for (int32_t y = 0; y < flic->height; ++y)
    {
        for (int32_t x = 0; x < flic->width; ++x)
        {
            uint32_t pixelIndex = y * frame->rowStride + x;
            if (frame->pixels[pixelIndex] & FLI_PXL_CHANGE)
            {
                uint8_t index = frame->pixels[pixelIndex] & FLI_PXL_INDEX;
                uint32_t imageIndex = (y * flic->width + x) * 3;
                image[imageIndex + 0] = frame->colors[index * 3 + 0];
                image[imageIndex + 1] = frame->colors[index * 3 + 1];
                image[imageIndex + 2] = frame->colors[index * 3 + 2];
            }
        }
    }
}

void flic_rewind(Flic* flic)
{
    if (!flic || !flic->file)
        return;

    flic->currentFrame = 0;
    flic->lastFrameDelay = 0;
    if (flic->oframe1)
    {
        flic__seek(flic->file, flic->oframe1);
    }
}

#endif // SHL_FLIC_IMPLEMENTATION
#endif // SHL_FLIC_H
