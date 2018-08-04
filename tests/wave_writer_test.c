#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SHL_WAVE_WRITER_IMPLEMENTATION
#include "../wave_writer.h"

#define PI 3.14159265359f

int main(int argc, char **argv)
{
    shlWaveFile waveFile;

    long sampleRate = 44100;
    long toneHz = 256;
    long wavePeriod = sampleRate / toneHz;
    long bytesPerSample = sizeof (shl_sample_t);
    long toneVolume = 3000;

    if (!shlWaveInit(&waveFile, sampleRate, "out.wav")) {
    	printf("Couldn't init wave file");
    	return -1;
    }

    long length = sampleRate * 60;
    shl_sample_t *buffer = (shl_sample_t*) malloc(length * bytesPerSample);

    for (int i = 0; i < length; i++)
    {
        float t = 2.0f * PI * (float)i / (float)wavePeriod;
        float sineValue = sinf(t);
        float sampleValue = sineValue * toneVolume;
        buffer[i] = (shl_sample_t)sampleValue;
    }

    shlWaveWrite(&waveFile, buffer, length, 1);
    shlWaveFlush(&waveFile, true);

    return 0;
}
