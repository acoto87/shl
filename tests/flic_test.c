#include <stdio.h>
#include <stdint.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define SHL_FLIC_IMPLEMENTATION
#include "../flic.h"

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("Need path to FLIC file\n");
        return -1;
    }

    const char* fileName = argv[1];

    Flic flic;
    if (!flicOpen(&flic, fileName))
    {
        printf("Could not open the FLIC file\n");
        return -1;
    }

    // accumulative image data
    uint8_t* imageData = (uint8_t*)calloc(flic.width * flic.height * 3, sizeof(uint8_t));

    for (int i = 0; i < flic.frames; i++)
    {
        FlicFrame frame;
        frame.pixels = (uint16_t*)calloc(flic.width * flic.height, sizeof(uint16_t));
        frame.rowStride = flic.width;
        if (flicReadFrame(&flic, &frame))
        {
            flicMakeImage(&flic, &frame, imageData);

            char imageFileName[16];
            sprintf(imageFileName, "frame%02d.bmp", i);
            stbi_write_bmp(imageFileName, flic.width, flic.height, 3, imageData);
        }
    }

    flicClose(&flic);

    return 0;
}