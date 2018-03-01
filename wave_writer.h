// A single header library version in C89 of the Wave_Writer library written by Shay Green.

// WAVE sound file writer for recording 16-bit output during program development

/* 
Copyright (C) 2003-2005 by Shay Green. Permission is hereby granted, free
of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
*/

#ifndef INCLUDE_SHL_WAVE_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef short shl_sample_t;
typedef enum { SHL_FALSE, SHL_TRUE } shl_bool_t;

#define SHL_WAVE_BUFFER_SIZE (32768 * 2)
#define SHL_WAVE_HEADER_SIZE 0x2C

#define SHL_U8_CAST(x) ((unsigned char)(x))

typedef struct _shl_wave_file
{
	FILE*   		_file;
	unsigned char*  _buffer;
	long    		sample_rate;
	long    		_sample_count;
	long    		_buffer_pos;
	int     		channel_count;

	void* 			(*mem_alloc)( size_t );
	void 			(*mem_free)( void* );
} shl_wave_file; 

extern shl_bool_t shl_wave_init( shl_wave_file *wave_file, long sample_rate, const char* filename, void* (*mem_alloc)(size_t), void (*mem_free)(void*) );
extern void 	  shl_wave_stereo( shl_wave_file *wave_file, int stereo );
extern long 	  shl_wave_sample_count( shl_wave_file *wave_file );
extern shl_bool_t shl_wave_write( shl_wave_file *wave_file, const shl_sample_t *in, long count, int skip );
extern shl_bool_t shl_wave_flush( shl_wave_file *wave_file, shl_bool_t close_file );

#ifdef __cplusplus
}
#endif

#ifdef SHL_WAVE_WRITER_IMPLEMENTATION

static shl_bool_t shl__wave_flush( shl_wave_file *wave_file )
{
    if ( wave_file->_buffer_pos && !fwrite( wave_file->_buffer, wave_file->_buffer_pos, 1, wave_file->_file ) ) {
        return SHL_FALSE;
    }

    wave_file->_buffer_pos = 0;
    return SHL_TRUE;
}

shl_bool_t shl_wave_init( shl_wave_file *wave_file, long sample_rate, const char* filename, void* (*mem_alloc)(size_t), void (*mem_free)(void*) )
{
    wave_file->mem_alloc = mem_alloc;
    if ( !wave_file->mem_alloc ) {
        wave_file->mem_alloc = &malloc;
    }

    wave_file->mem_free = mem_free;
    if ( !wave_file->mem_free ) {
        wave_file->mem_free = &free;
    }

    wave_file->_buffer = (unsigned char*) wave_file->mem_alloc(SHL_WAVE_BUFFER_SIZE);	
    if ( !wave_file->_buffer ) {
        return SHL_FALSE;
    }

    wave_file->_file = fopen(filename, "wb");
    if ( !wave_file->_file ) {
        return SHL_FALSE;
    }

    wave_file->sample_rate = sample_rate;
    wave_file->channel_count = 1;

    wave_file->_sample_count = 0;
    wave_file->_buffer_pos = SHL_WAVE_HEADER_SIZE;

    return SHL_TRUE;
}

void shl_wave_stereo( shl_wave_file *wave_file, int stereo )
{
    wave_file->channel_count = stereo ? 2 : 1;
}

long shl_wave_sample_count( shl_wave_file *wave_file )
{
    return wave_file->_sample_count;
}

shl_bool_t shl_wave_write( shl_wave_file *wave_file, const shl_sample_t *in, long count, int skip )
{
    wave_file->_sample_count += count;
    while ( count ) {
        if ( wave_file->_buffer_pos >= SHL_WAVE_BUFFER_SIZE ) {
            if ( !shl__wave_flush( wave_file ) ) {
                return SHL_FALSE;
            }
        }

        long n = (unsigned long) (SHL_WAVE_BUFFER_SIZE - wave_file->_buffer_pos) / sizeof (shl_sample_t);
        if ( n > count ) n = count;
        count -= n;

    // convert to lsb first format
        unsigned char* p = &wave_file->_buffer[wave_file->_buffer_pos];
        while ( n-- ) {
            int s = *in;
            in += skip;
            *p++ = (unsigned char) s;
            *p++ = (unsigned char) (s >> 8);
        }

        wave_file->_buffer_pos = p - wave_file->_buffer;
    }

    return SHL_TRUE;
}

shl_bool_t shl_wave_flush( shl_wave_file *wave_file, int close_file )
{
    if ( !shl__wave_flush ( wave_file ) ) {
        return SHL_FALSE;
    }

    if ( close_file ) {
        // generate header
        long sample_rate = wave_file->sample_rate;
        int channel_count = wave_file->channel_count;
        long ds = wave_file->_sample_count * sizeof (shl_sample_t);
        long rs = SHL_WAVE_HEADER_SIZE - 8 + ds;
        int frame_size = channel_count * sizeof (shl_sample_t);
        long bps = sample_rate * frame_size;

        unsigned char header[SHL_WAVE_HEADER_SIZE] = {
            'R','I','F','F',
            SHL_U8_CAST(rs), SHL_U8_CAST(rs >> 8),                               // length of rest of file
            SHL_U8_CAST(rs >> 16), SHL_U8_CAST(rs >> 24),
            'W','A','V','E',
            'f','m','t',' ',
            0x10, 0, 0, 0,                                                       // size of fmt chunk
            1, 0,                                                                // uncompressed format
            SHL_U8_CAST(channel_count), 0,                                       // channel count
            SHL_U8_CAST(sample_rate), SHL_U8_CAST(sample_rate >> 8),             // sample rate
            SHL_U8_CAST(sample_rate >> 16), SHL_U8_CAST(sample_rate >> 24),
            SHL_U8_CAST(bps), SHL_U8_CAST(bps >> 8),                             // bytes per second
            SHL_U8_CAST(bps >> 16), SHL_U8_CAST(bps >> 24),
            SHL_U8_CAST(frame_size), 0,                                          // bytes per sample frame
            16, 0,                                                               // bits per sample
            'd','a','t','a',
            SHL_U8_CAST(ds), SHL_U8_CAST(ds >> 8),                               // size of sample data
            SHL_U8_CAST(ds >> 16), SHL_U8_CAST(ds >> 24)
            // ...              												// sample data
        };

        // write header
        if ( fseek( wave_file->_file, 0, SEEK_SET ) ) {
            return SHL_FALSE;
        }

        if ( !fwrite( header, sizeof header, 1, wave_file->_file ) ) {
            return SHL_FALSE;
        }

        if ( fclose( wave_file->_file ) ) {
            return SHL_FALSE;
        }

        wave_file->mem_free (wave_file->_buffer);
    }

    return SHL_TRUE;
}

#endif // SHL_WAVE_WRITER_IMPLEMENTATION

#ifdef SHL_WAVE_WRITER_TEST

static void * my_mem_alloc( size_t size ) {
    return malloc( size );
}

int main( int argc, char **argv )
{
    shl_wave_file wave_file = {};

    long sample_rate = 44100;
    long tone_hz = 256;
    long wave_period = sample_rate / tone_hz;
    long bytes_per_sample = sizeof (shl_sample_t);
    long tone_volume = 3000;

    if ( !shl_wave_init( &wave_file, sample_rate, "out.wav", &my_mem_alloc, 0 ) ) {
    	printf("Couldn't init wave file");
    	return -1;
    }

#define PI 3.14159265359f

    long length = sample_rate * 60;
    shl_sample_t *buffer = (shl_sample_t*) my_mem_alloc(length * bytes_per_sample);

    for (int i = 0; i < length; i++)
    {
        float t = 2.0f * PI * (float)i / (float)wave_period;
        float sine_value = sinf(t);
        float sample_value = sine_value * tone_volume;
        buffer[i] = (shl_sample_t)sample_value;
    }

    shl_wave_write( &wave_file, buffer, length, 1 );
    shl_wave_flush( &wave_file, true );

    return 0;
}

#endif

#endif // INCLUDE_SHL_WAVE_WRITER_H