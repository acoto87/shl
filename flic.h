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

    This implementation is a C port of the following C++ implementation by David Capello
    Aseprite FLIC Library: https://github.com/aseprite/flic
*/
#ifndef SHL_FLIC_H
#define SHL_FLIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
#define FLI_COLORS_SIZE         (256*3)

typedef struct _shlFlic {
    uint16_t frames;
    uint16_t width;
    uint16_t height;
    uint16_t speed;
    
    FILE* file;
    uint32_t oframe1;
    uint32_t oframe2;
    uint32_t currentFrame;
} shlFlic;

typedef struct _shlFlicFrame {
    uint8_t* pixels;
    uint32_t rowStride;
    uint8_t colors[FLI_COLORS_SIZE];
} shlFlicFrame;

bool flicOpen(shlFlic* flic, const char* filename);
bool flicReadFrame(shlFlic* flic, shlFlicFrame* frame);
void flicClose(shlFlic* flic);

#ifdef __cplusplus
}
#endif

#ifdef SHL_FLIC_IMPLEMENTATION

static inline uint8_t flic__read8(FILE* file)
{
    return fgetc(file);
}

static inline void flic__write8(FILE* file, uint8_t value)
{
    fputc(value, file);
}

static inline uint16_t flic__read16(FILE* file)
{
    uint8_t b1 = flic__read8(file);
    if (b1 == EOF) return 0;
    uint8_t b2 = flic__read8(file);
    if (b2 == EOF) return 0;

    return (b2 << 8) | b1;
}

static inline uint32_t flic__read32(FILE* file)
{
    uint8_t b1 = flic__read8(file);
    if (b1 == EOF) return 0;
    uint8_t b2 = flic__read8(file);
    if (b2 == EOF) return 0;
    uint8_t b3 = flic__read8(file);
    if (b3 == EOF) return 0;
    uint8_t b4 = flic__read8(file);
    if (b4 == EOF) return 0;

    return (b4 << 24) | (b3 << 16) | (b2 << 8) | b1;
}

static inline size_t flic__tell(FILE* file)
{
    return ftell(file);
}

static inline void flic__seek(FILE* file, size_t pos)
{
    fseek(file, pos, SEEK_SET);
}

static void flic__readBlackChunk(shlFlic* flic, shlFlicFrame* frame)
{
    memset(frame->pixels, 0, frame->rowStride * flic->height);
}

static void flic__readCopyChunk(shlFlic* flic, shlFlicFrame* frame)
{
    for (int32_t y = 0; y < flic->height; ++y) 
    {
        uint8_t* row = frame->pixels + y * frame->rowStride;
        for (int32_t x = 0; x < flic->width; ++x)
            row[x] = flic__read8(flic->file);
    }
}

static void flic__readColorChunk(shlFlic* flic, shlFlicFrame* frame, bool is64ColorMap)
{
    uint16_t npackets = flic__read16(flic->file);

    uint8_t i = 0;
    while (npackets--) 
    {
        i += flic__read8(flic->file); // Colors to skip

        uint16_t colors = (uint16_t)flic__read8(flic->file);
        if (colors == 0)
            colors = FLI_COLORS_SIZE;

        for (int32_t j = 0; j < colors; ++j) 
        {
            uint8_t r = flic__read8(flic->file);
            uint8_t g =  flic__read8(flic->file);
            uint8_t b =  flic__read8(flic->file);
            
            if (is64ColorMap) {
                r *=(uint8_t)(255 * ((float)r / 63));
                g = (uint8_t)(255 * ((float)g / 63));
                b = (uint8_t)(255 * ((float)b / 63));
            }

            frame->colors[(i + j) * 3 + 0] = r;
            frame->colors[(i + j) * 3 + 1] = g;
            frame->colors[(i + j) * 3 + 2] = b;
        }
    }
}

static void flic__readBrunChunk(shlFlic* flic, shlFlicFrame* frame)
{
    for (int32_t y = 0; y < flic->height; ++y) 
    {
        uint8_t* row = frame->pixels + frame->rowStride * y;

        int32_t x = 0;
        flic__read8(flic->file); // Ignore number of packets (we read until x == m_width)

        while (x < flic->width) 
        {
            int8_t count = (int8_t)flic__read8(flic->file);
            if (count >= 0) 
            {
                uint8_t color = flic__read8(flic->file);
                while (count-- && x < flic->width) 
                    row[x++] = color;
            }
            else 
            {
                count = -count;
                while (count--) 
                    row[x++] = flic__read8(flic->file);
            }
        }
    }
}

static void flic__readLcChunk(shlFlic* flic, shlFlicFrame* frame)
{
    uint16_t skipLines = flic__read16(flic->file);
    uint16_t nlines = flic__read16(flic->file);

    for (int32_t y = skipLines; y < skipLines + nlines; ++y) {
        uint8_t* row = frame->pixels + frame->rowStride * y;

        int32_t x = 0;
        uint8_t npackets = flic__read8(flic->file);
        while (npackets-- && x < flic->width) 
        {
            uint8_t skip = flic__read8(flic->file);

            x += skip;

            int8_t count = (int8_t)flic__read8(flic->file);
            if (count >= 0) 
            {
                while (count--)
                    row[x++] = flic__read8(flic->file);
            }
            else 
            {
                count = -count;

                uint8_t color = flic__read8(flic->file);
                while (count-- && x < flic->width) 
                    row[x++] = color;
            }
        }
    }
}

static void flic__readDeltaChunk(shlFlic* flic, shlFlicFrame* frame)
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
                assert(y >= 0 && y < flic->height);
                if (y >= 0 && y < flic->height) 
                {
                    frame->pixels[y * frame->rowStride + flic->width - 1] = (word & 0xff);
                }

                ++y;
                
                if (nlines-- == 0)
                    return;
            }

            word = (int16_t)flic__read16(flic->file);
        }

        uint16_t npackets = (uint16_t)word;

        int32_t x = 0;

        while (npackets--) 
        {
            x += flic__read8(flic->file); // Skip pixels

            int8_t count = (int8_t)flic__read8(flic->file); // Number of words

            assert(y >= 0 && y < flic->height && x >= 0 && x < flic->width);
            uint8_t* row = frame->pixels + y * frame->rowStride + x;

            if (count >= 0) 
            {
                while (count-- && x < flic->width) 
                {
                    uint8_t color1 = flic__read8(flic->file);
                    uint8_t color2 = flic__read8(flic->file);

                    row[x++] = color1;

                    if (x < flic->width) 
                        row[x++] = color2;
                }
            }
            else 
            {
                count = -count;

                uint8_t color1 = flic__read8(flic->file);
                uint8_t color2 = flic__read8(flic->file);

                while (count-- && x < flic->width) 
                {
                    row[x++] = color1;

                    if (x < flic->width) 
                        row[x++] = color2;;
                }
            }
        }

        ++y;
    }
}

static void flic__readChunk(shlFlic* flic, shlFlicFrame* frame)
{
    uint32_t chunkStartPos = flic__tell(flic->file);
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

bool flicOpen(shlFlic* flic, const char* filename)
{
    memset(flic, 0, sizeof(shlFlic));

    flic->file = fopen(filename, "rb");

    flic__read32(flic->file); // file size
    uint16_t magic = flic__read16(flic->file);

    assert(magic == FLI_MAGIC_NUMBER || magic == FLC_MAGIC_NUMBER);
    if (magic != FLI_MAGIC_NUMBER && magic != FLC_MAGIC_NUMBER)
        return false;

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
  }

    if (magic == FLC_MAGIC_NUMBER) 
    {
        // Offset to the first and second frame header values
        flic__seek(flic->file, 80);

        flic->oframe1 = flic__read32(flic->file);
        flic->oframe2 = flic__read32(flic->file);
    }

    if (flic->width == 0) flic->width = 320;
    if (flic->height == 0) flic->width = 200;

    // Skip padding
    flic__seek(flic->file, 128);
    return true;
}

void flicClose(shlFlic* flic)
{
    fclose(flic->file);
}

bool flicReadFrame(shlFlic* flic, shlFlicFrame* frame)
{
    switch (flic->currentFrame) {
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

    uint32_t frameStartPos = flic__tell(flic->file);
    uint32_t frameSize = flic__read32(flic->file);
    uint16_t magic = flic__read16(flic->file);
    assert(magic == FLI_FRAME_MAGIC_NUMBER);
  
    uint16_t chunks = flic__read16(flic->file);
    for (int i = 0; i < 8; ++i)       // Padding
        flic__read8(flic->file);

    for (uint16_t i = 0; i < chunks; ++i)
        flic__readChunk(flic, frame);

    flic__seek(flic->file, frameStartPos + frameSize);
    flic->currentFrame++;
    return true;
}

#endif // SHL_FLIC_IMPLEMENTATION
#endif // SHL_FLIC_H