#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

#include "spectrum.c"
#include "render.c"

typedef struct application_state_t
{
    int is_running;
    int is_fullscreen;
    int windowed_w;
    int windowed_h;
    int loop_flag;

    Font main_font;

    spectrum_state_t spectrum_state;
} application_state_t;

static void
handle_input(application_state_t *application_state)
{
    if (IsKeyPressed(KEY_F11))
    {
        if (IsWindowFullscreen())
        {
            ToggleFullscreen();
            SetWindowSize(application_state->windowed_w, application_state->windowed_h);
        }
        else
        {
            application_state->windowed_w = GetScreenWidth();
            application_state->windowed_h = GetScreenHeight();
            int monitor = GetCurrentMonitor();
            int mw = GetMonitorWidth(monitor);
            int mh = GetMonitorHeight(monitor);
            SetWindowSize(mw, mh);
            ToggleFullscreen();
        }
        spectrum_handle_resize(&application_state->spectrum_state);
    }
}

int main(int argc, char **argv)
{
    application_state_t *application_state = (application_state_t *)calloc(1, sizeof(application_state_t));
    if (!application_state)
    {
        fprintf(stderr, "Failed to allocate application state\n");
        return 1;
    }

    const char *input_file = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--loop") == 0 || strcmp(argv[i], "-l") == 0)
        {
            application_state->loop_flag = 1;
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

    application_state->main_font = LoadFontEx("assets/fonts/retro-pixel-arcade.ttf", 64, 0, 250);

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
    spectrum_init(&application_state->spectrum_state, &wave, application_state->main_font);
    spectrum_set_total_windows(&application_state->spectrum_state, wave.frameCount / FFT_WINDOW_SIZE);

    application_state->windowed_w = WINDOW_WIDTH;
    application_state->windowed_h = WINDOW_HEIGHT;

    while (!WindowShouldClose())
    {
        double dt = GetFrameTime();

        handle_input(application_state);

        spectrum_handle_resize(&application_state->spectrum_state);
        spectrum_update(&application_state->spectrum_state, &wave, samples, dt);
        spectrum_render_to_texture(&application_state->spectrum_state);

        if (!application_state->loop_flag && spectrum_done(&application_state->spectrum_state))
        {
            break;
        }
        if (application_state->loop_flag && spectrum_done(&application_state->spectrum_state))
        {
            application_state->spectrum_state.window_index = 0;
            application_state->spectrum_state.accumulator = 0.0;
        }
        if (application_state->loop_flag && !IsSoundPlaying(sound))
        {
            PlaySound(sound);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        render_draw(&application_state->spectrum_state);
        EndDrawing();
    }

    UnloadWaveSamples(samples);
    UnloadWave(wave);
    StopSound(sound);
    UnloadSound(sound);
    spectrum_destroy(&application_state->spectrum_state);
    CloseAudioDevice();
    CloseWindow();

    free(application_state);

    return 0;
}
