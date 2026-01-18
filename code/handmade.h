#if !defined(HANDMADE_H)

// TODO: Implement sine ourselves
#include <math.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

/*
  TODO: Services that the platform later provides to the game.
*/

/*
  NOTE: Services that the game provides to the platform layer.
  (this my expand in the future - sound on separate thread, etc.)
*/

// FOUR THINGS - timing,  controller/keyboard input, bitmap buffer to use, sound buffer to use

// TODO: In the future, rendering _specifically_ will becomebeome a three-listed abstraction!!!
struct game_offscreen_buffer
{
    // NOTE: Pixels are always 32-bits wide, Memory Order BB GG RR xx
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer,
                                  int BlueOffset, int GreenOffset,
                                  game_sound_output_buffer *SoundBuffer, int ToneHz);

#define HANDMADE_H
#endif