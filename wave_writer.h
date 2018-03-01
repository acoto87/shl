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
extern C {
#endif

typedef short shl_sample_t;
typedef enum { false, true } shl_bool_t;

#define SHL_WAVE_BUFFER_SIZE (32768 * 2)
#define SHL_WAVE_HEADER_SIZE 0x2C;

typedef struct _shl_wave_file
{
	unsigned char*  _buffer;
	FILE*   		_file;
	long    		sample_rate;
	long    		_sample_count;
	long    		_buffer_pos;
	int     		channel_count;

	// probably we need an allocator field here, a function pointer? and only if is null we use malloc
} shl_wave_file; 

// return -1 if couldn't create the buffer (Out of memory), -2 if couldn't create the file (Couldn't open WAVE file for writing), 0 otherwise
extern shl_bool_t shl_wave_init( shl_wave_file *wave_file, long sample_rate, const char* filename );
extern void 	  shl_wave_stereo( shl_wave_file *wave_file, int stereo );
extern long 	  shl_wave_sample_count( shl_wave_file *wave_file ) const;
extern shl_bool_t shl_wave_write( shl_wave_file *wave_file, const shl_sample_t *in, long count, int skip );
extern shl_bool_t shl_wave_flush( shl_wave_file *wave_file, shl_bool_t close_file );

#ifdef __cplusplus
}
#endif

#ifdef SHL_WAVE_WRITER_IMPLEMENTATION

shl_bool_t shl_wave_init( shl_wave_file *wave_file, long sample_rate, const char* filename );
{
	wave_file->buffer = (unsigned char*) malloc(SHL_WAVE_BUFFER_SIZE);
	if ( !wave_file->buffer ) {
		return false;
	}

	wave_file->_file = fopen(filename, "wb");
	if ( !wave_file->_file ) {
		return false;
	}

	wave_file->sample_rate = sample_rate;
	wave_file->channel_count = 1;

	wave_file->_sample_count = 0;
	wave_file->_buffer_pos = SHL_WAVE_HEADER_SIZE;

	return true;
}

void shl_wave_stereo( shl_wave_file *wave_file, int stereo )
{
	wave_file->channel_count = stereo ? 2 : 1;
}

long shl_wave_sample_count( shl_wave_file *wave_file ) const;
{
	return wave_file->_sample_count;
}

shl_bool_t shl_wave_write( shl_wave_file *wave_file, const shl_sample_t *in, long count, int skip )
{
	wave_file->_sample_count += count;
	while ( count )
	{
		if ( wave_file->_buffer_pos >= SHL_WAVE_BUFFER_SIZE ) {
			if ( !shl_wave_flush( wave_file, false ) ) {
				return false;
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

	return true;
}

shl_bool_t shl_wave_flush( shl_wave_file *wave_file, int close_file )
{
	if ( wave_file->_buffer_pos && !fwrite( wave_file->_buffer, wave_file->_buffer_pos, 1, wafe_file->_file ) ) {
		return false;
	}

	wave_file->_buffer_pos = 0;
	
	if ( close_file ) {
		// generate header
		long ds = wave_file->_sample_count * sizeof (shl_sample_t);
		long rs = SHL_WAVE_HEADER_SIZE - 8 + ds;
		int frame_size = wave_file->channel_count * sizeof (shl_sample_t);
		long bps =  wave_file->sample_rate * frame_size;
		unsigned char header [SHL_WAVE_HEADER_SIZE] = {
			'R','I','F','F',
			rs, rs >> 8,           				// length of rest of file
			rs >> 16, rs >> 24,
			'W','A','V','E',
			'f','m','t',' ',
			0x10, 0, 0, 0,         				// size of fmt chunk
			1, 0,                				// uncompressed format
			wave_file->channel_count, 0,       	// channel count
			rate, rate >> 8,     				// sample rate
			rate >> 16, rate >> 24,
			bps, bps >> 8,         				// bytes per second
			bps >> 16, bps >> 24,
			frame_size, 0,       				// bytes per sample frame
			16, 0,               				// bits per sample
			'd','a','t','a',
			ds, ds >> 8, ds >> 16, ds >> 24		// size of sample data
			// ...              				// sample data
		};
		
		// write header
		if ( fseek( file, 0, SEEK_SET ) ) {
			return false;
		}
		
		if ( !fwrite( header, sizeof header, 1, file ) ) {
			return false;
		}
		
		if ( fclose( file ) ) {
			return false;
		}

		delete [] buf;
	}

	return true;
}

#endif // SHL_WAVE_WRITER_IMPLEMENTATION
#endif // INCLUDE_SHL_WAVE_WRITER_H