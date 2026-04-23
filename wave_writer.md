# Wave writer

`wave_writer.h` writes 16-bit PCM `.wav` files incrementally. It keeps sample data in an internal buffer and writes the RIFF/WAVE header when the stream is flushed and closed.

Define `SHL_WAV_IMPLEMENTATION` in exactly one translation unit before including `wave_writer.h` to compile the implementation.

```c
#define SHL_WAV_IMPLEMENTATION
#include "wave_writer.h"
```

## Types and constants

| Name | Description |
| --- | --- |
| `wav_sample_t` | Sample type written to disk. It is a signed 16-bit sample stored as `short`. |
| `wav_file_t` | Streaming writer state holding the file handle, temporary buffer, sample rate, sample count, and channel count. |
| `WAV_BUFFER_SIZE` | Size in bytes of the internal output buffer. |
| `WAV_HEADER_SIZE` | Size in bytes of the WAVE header reserved at the start of the file. |

## API

| Function | Description | Return type |
| --- | --- | --- |
| `wav_init`(wav_file_t* waveFile, long sampleRate, const char* filename) | Opens `filename`, allocates the internal buffer, and starts a mono stream at `sampleRate`. | `bool` |
| `wav_stereo`(wav_file_t* waveFile, bool stereo) | Sets the channel count to mono (`false`) or stereo (`true`). Call this before finalizing the file. | `void` |
| `wav_sampleCount`(wav_file_t* waveFile) | Returns the number of samples written so far. This is a sample count, not a frame count. | `long` |
| `wav_write`(wav_file_t* waveFile, const wav_sample_t* in, long count, int skip) | Writes `count` samples from `in`, advancing the input pointer by `skip` elements after each sample. | `bool` |
| `wav_flush`(wav_file_t* waveFile, bool closeFile) | Flushes buffered data. When `closeFile` is `true`, also writes the final WAVE header, closes the file, and frees the internal buffer. | `bool` |

Notes:

* The writer always emits 16-bit PCM sample data.
* `wav_init()` starts in mono mode. Call `wav_stereo(&waveFile, true)` to emit a stereo header.
* `wav_flush(&waveFile, false)` only flushes pending sample bytes. The file is not a complete WAVE file until `wav_flush(&waveFile, true)` succeeds.

Example:

```c
#define SHL_WAV_IMPLEMENTATION
#include "wave_writer.h"

int main(void)
{
    wav_file_t waveFile = {0};
    wav_sample_t samples[] = { 0, 1000, -1000, 0 };

    if (!wav_init(&waveFile, 44100, "out.wav"))
        return 1;

    if (!wav_write(&waveFile, samples, 4, 1))
        return 1;

    if (!wav_flush(&waveFile, true))
        return 1;

    return 0;
}
```
