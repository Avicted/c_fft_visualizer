#include <raylib.h>
#include <stdio.h>
#include <math.h>
#include <fftw3.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FFT_WINDOW_SIZE 1024

int main(void)
{
    printf("FFT Visualizer Starting...\n");

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);

    const char *input_file_path = "m.wav";

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

    Sound sound = LoadSound(input_file_path);
    PlaySound(sound);

    float *samples = LoadWaveSamples(wave);
    float mono_buffer[FFT_WINDOW_SIZE];

    int total_windows = wave.frameCount / FFT_WINDOW_SIZE;
    printf("Total FFT windows: %d (size %d)\n", total_windows, FFT_WINDOW_SIZE);

    double in[FFT_WINDOW_SIZE];
    fftw_complex out[FFT_WINDOW_SIZE / 2 + 1];
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_WINDOW_SIZE, in, out, FFTW_ESTIMATE);

    double smoothed[FFT_WINDOW_SIZE / 2 + 1] = {0};
    double target[FFT_WINDOW_SIZE / 2 + 1] = {0};
    double lerpSpeed = 0.20;

    float seconds_per_window = (float)FFT_WINDOW_SIZE / wave.sampleRate;
    int w = 0;
    float accumulator = 0.0f;

    while (!WindowShouldClose() && w < total_windows)
    {
        float dt = GetFrameTime();
        accumulator += dt;

        if (accumulator >= seconds_per_window)
        {
            accumulator -= seconds_per_window;

            int start_index = w * FFT_WINDOW_SIZE * wave.channels;

            for (int i = 0; i < FFT_WINDOW_SIZE; i++)
            {
                if (wave.channels == 1)
                {
                    mono_buffer[i] = samples[start_index + i];
                }
                else
                {
                    mono_buffer[i] = 0.5f * (samples[start_index + (i * 2)] + samples[start_index + (i * 2) + 1]);
                }

                in[i] = (double)mono_buffer[i];
            }

            fftw_execute(plan);

            for (int i = 0; i < FFT_WINDOW_SIZE / 2 + 1; i++)
            {
                target[i] = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]) / FFT_WINDOW_SIZE;
            }

            w++;
        }

        for (int i = 0; i < FFT_WINDOW_SIZE / 2 + 1; i++)
        {
            smoothed[i] += lerpSpeed * (target[i] - smoothed[i]);
        }

        BeginDrawing();
        ClearBackground(BLACK);

        int num_bins = FFT_WINDOW_SIZE / 2 + 1;
        float bar_width = (float)WINDOW_WIDTH / num_bins;

        for (int i = 0; i < num_bins; i++)
        {
            float mag_dB = 20.0f * log10(smoothed[i] + 1e-6);
            const float min_dB = -80.0f;
            const float max_dB = 0.0f;

            if (mag_dB < min_dB)
            {
                mag_dB = min_dB;
            }

            float bar_height = (mag_dB - min_dB) / (max_dB - min_dB) * WINDOW_HEIGHT;

            if (bar_height > WINDOW_HEIGHT)
                bar_height = WINDOW_HEIGHT;

            DrawRectangle((int)(i * bar_width),
                          WINDOW_HEIGHT - (int)bar_height,
                          (int)bar_width,
                          (int)bar_height,
                          GREEN);
        }

        DrawText("FFT Visualizer", 10, 10, 20, WHITE);
        EndDrawing();
    }

    fftw_destroy_plan(plan);
    UnloadWaveSamples(samples);
    UnloadWave(wave);
    StopSound(sound);
    UnloadSound(sound);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
