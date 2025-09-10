#include "macros.h"

#include <raylib.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

int main(void)
{
    printf("Hello Sailor!\n");

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);

    const char *input_file_path = "Firefly_in_a_fairytale_by_Gareth_Coker.wav";

    Wave wave = LoadWave(input_file_path);
    if (wave.frameCount == 0)
    {
        fprintf(stderr, "Failed to load WAV: %s\n", input_file_path);
        return 1;
    }

    printf("Loaded WAV file: %s\n", input_file_path);
    printf("  Sample rate: %d Hz\n", wave.sampleRate);
    printf("  Channels:    %d\n", wave.channels);
    printf("  Bits/sample: %d\n", wave.sampleSize);
    printf("  Frames:      %d\n", wave.frameCount);

    float *samples = LoadWaveSamples(wave);

    printf("First 10000 samples (ch0):\n");
    for (int i = 0; i < 10000; i++)
    {
        float sample = samples[i * wave.channels];
        if (sample == 0.0f)
        {
            continue;
        }

        printf("%f\n", sample);
    }

    // while (!WindowShouldClose())
    // {
    //     BeginDrawing();
    //     ClearBackground(BLACK);
    //     DrawText("Hello, world!", 10, 10, 20, WHITE);
    //     EndDrawing();
    // }

    UnloadWaveSamples(samples);
    UnloadWave(wave);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
