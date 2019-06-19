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

    shlFlic flic;
    flicOpen(&flic, fileName);

    for (int i = 0; i < flic.frames; i++)
    {
        shlFlicFrame frame;
        frame.pixels = (uint8_t*)calloc(flic.width * flic.height, sizeof(uint8_t));
        frame.rowStride = flic.width;
        if (flicReadFrame(&flic, &frame))
        {
            printf("------- pixels -------\n");
            for (int y = 0; y < flic.height; y++)
            {
                for (int x = 0; x < flic.width; x++)
                {
                    printf("%d ", frame.pixels[y * flic.width + x]);
                }

                printf("\n");
            }

            printf("------- colors -------\n");
            for (int j = 0; j < FLI_COLORS_SIZE/3; j++)
                printf("(%d, %d, %d)\n", frame.colors[j*3+0], frame.colors[j*3+1], frame.colors[j*3+2]);
        }
    }

    flicClose(&flic);

    return 0;
}