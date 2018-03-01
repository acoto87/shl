
#include <stdio.h>
#include <math.h>

#define SHL_WAVE_WRITER_IMPLEMENTATION
#include "../wave_writer.h"

#define PI 3.14159265359f

int main(int argc, char **argv)
{
	wave_file *wave_file;

	long sample_rate = 44100;
	long tone_hz = 256;
	long wave_period = sample_rate / tone_hz;
    long bytes_per_sample = sizeof (shl_sample_t);
    long tone_volume = 3000;
    long wave_pos = 0;

	if ( !shl_wave_init( wave_file, sample_rate, "out.wav" ) ) {
		printf("Couldn't init wave file");
		return -1;
	}

	long length = sample_rate * 60;
	shl_sample_t buffer[length];

    for (int i = 0; i < length; i++)
    {
        float t = 2.0f * PI * (float)i / (float)wave_period;
        float sine_value = sinf(t);
        float sample_value = sine_value * tone_volume;
        buffer[i] = (shl_sample_t)sample_value;
    }

    shl_wave_flush( wave_file, true );

	return 0;
}