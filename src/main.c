#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

#include "spectrum.c"
#include "render.c"

int main(int argc, char **argv)
{
    const char *input_file = NULL;
    int loop_flag = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--loop") == 0 || strcmp(argv[i], "-l") == 0)
        {
            loop_flag = 1;
        }
        else if (!input_file)
        {
            input_file = argv[i];
        }
    }
    if (!input_file)
    {
        fprintf(stderr, "Usage: %s <wav file> [--loop]\n", argv[0]);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);
    SetWindowIcon(LoadImage("assets/icon.png"));

    Font main_font = LoadFontEx("assets/fonts/retro-pixel-arcade.ttf", 64, 0, 250);
    Wave wave = LoadWave(input_file);
    if (wave.frameCount == 0)
    {
        fprintf(stderr, "Failed to load WAV: %s\n", input_file);
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    Sound sound = LoadSound(input_file);
    PlaySound(sound);

    float *samples = LoadWaveSamples(wave);
    spectrum_state state;
    spectrum_init(&state, &wave, main_font);
    spectrum_set_total_windows(&state, wave.frameCount / FFT_WINDOW_SIZE);

    int windowed_w = WINDOW_WIDTH;
    int windowed_h = WINDOW_HEIGHT;

    while (!WindowShouldClose())
    {
        double dt = GetFrameTime();

        if (IsKeyPressed(KEY_F11))
        {
            if (IsWindowFullscreen())
            {
                ToggleFullscreen();
                SetWindowSize(windowed_w, windowed_h);
            }
            else
            {
                windowed_w = GetScreenWidth();
                windowed_h = GetScreenHeight();
                int monitor = GetCurrentMonitor();
                int mw = GetMonitorWidth(monitor);
                int mh = GetMonitorHeight(monitor);
                SetWindowSize(mw, mh);
                ToggleFullscreen();
            }
            spectrum_handle_resize(&state);
        }

        spectrum_handle_resize(&state);
        spectrum_update(&state, &wave, samples, dt);
        spectrum_render_to_texture(&state);

        if (!loop_flag && spectrum_done(&state))
        {
            break;
        }
        if (loop_flag && spectrum_done(&state))
        {
            state.window_index = 0;
            state.accumulator = 0.0;
        }
        if (loop_flag && !IsSoundPlaying(sound))
        {
            PlaySound(sound);
        }

        BeginDrawing();
        ClearBackground((Color){18, 18, 18, 255});
        render_draw(&state);
        EndDrawing();
    }

    UnloadWaveSamples(samples);
    UnloadWave(wave);
    StopSound(sound);
    UnloadSound(sound);
    spectrum_destroy(&state);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
