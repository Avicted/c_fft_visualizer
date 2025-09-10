#include "macros.h"

#include <raylib.h>

#include <stdio.h>
#include <math.h>
#include <fftw3.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FFT_WINDOW_SIZE 1024

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

    float mono_buffer[FFT_WINDOW_SIZE];

    int total_windows = wave.frameCount / FFT_WINDOW_SIZE;
    printf("Total FFT windows: %d (size %d)\n", total_windows, FFT_WINDOW_SIZE);

    double in[FFT_WINDOW_SIZE];
    fftw_complex out[FFT_WINDOW_SIZE / 2 + 1]; // r2c output size
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_WINDOW_SIZE, in, out, FFTW_ESTIMATE);

    for (int w = 0; w < total_windows; ++w)
    {
        int start_index = w * FFT_WINDOW_SIZE * wave.channels;

        for (int i = 0; i < FFT_WINDOW_SIZE; ++i)
        {
            if (wave.channels == 1)
            {
                // Mono, direct copy
                mono_buffer[i] = samples[start_index + i];
            }
            else if (wave.channels == 2)
            {
                // Stereo, average L+R
                float left = samples[start_index + (i * 2)];
                float right = samples[start_index + (i * 2) + 1];
                mono_buffer[i] = 0.5f * (left + right);
            }
        }

        // printf("Window %d: ", w);
        // for (int i = 0; i < 5; i++)
        // {
        //     printf("%+.4f ", mono_buffer[i]);
        // }
        // printf("...\n");

        // -----------------------------------
        for (int w = 0; w < total_windows && !WindowShouldClose(); w++)
        {
            int startIndex = w * FFT_WINDOW_SIZE * wave.channels;

            for (int i = 0; i < FFT_WINDOW_SIZE; ++i)
            {
                if (wave.channels == 1)
                {
                    mono_buffer[i] = samples[startIndex + i];
                }
                else
                {
                    float left = samples[startIndex + (i * 2)];
                    float right = samples[startIndex + (i * 2) + 1];
                    mono_buffer[i] = 0.5f * (left + right);
                }

                in[i] = (double)mono_buffer[i];
            }

            fftw_execute(plan);

            double magnitudes[FFT_WINDOW_SIZE / 2 + 1];
            for (int i = 0; i < FFT_WINDOW_SIZE / 2 + 1; ++i)
            {
                magnitudes[i] = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
            }

            BeginDrawing();
            ClearBackground(BLACK);

            int num_bins = 256; // FFT_WINDOW_SIZE / 2 + 1;
            float bar_width = (float)WINDOW_WIDTH / num_bins;

            for (int i = 0; i < num_bins; ++i)
            {
                const float scale_factor = 2.0f;
                float bar_height = (float)(magnitudes[i] * scale_factor);
                if (bar_height > WINDOW_HEIGHT)
                {
                    bar_height = WINDOW_HEIGHT;
                }

                DrawRectangle(
                    (int)(i * bar_width),
                    WINDOW_HEIGHT - (int)bar_height,
                    (int)bar_width,
                    (int)bar_height,
                    GREEN);
            }

            DrawText("FFT Visualizer", 10, 10, 20, WHITE);
            EndDrawing();
        }
    }

    fftw_destroy_plan(plan);

    UnloadWaveSamples(samples);
    UnloadWave(wave);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
