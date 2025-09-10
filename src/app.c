#include "app.h"

void app_parse_input_args(int argc, char **argv, char **input_file, int *loop_flag)
{
    *input_file = NULL;
    *loop_flag = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--loop") == 0 || strcmp(argv[i], "-l") == 0)
        {
            *loop_flag = 1;
        }
        else if (!*input_file)
        {
            *input_file = argv[i];
        }
    }
}

void app_handle_input(app_state_t *app_state)
{
    if (IsKeyPressed(KEY_F11))
    {
        if (IsWindowFullscreen())
        {
            ToggleFullscreen();
            SetWindowSize(app_state->windowed_w, app_state->windowed_h);
        }
        else
        {
            app_state->windowed_w = GetScreenWidth();
            app_state->windowed_h = GetScreenHeight();
            int monitor = GetCurrentMonitor();
            int mw = GetMonitorWidth(monitor);
            int mh = GetMonitorHeight(monitor);
            SetWindowSize(mw, mh);
            ToggleFullscreen();
        }
        spectrum_handle_resize(&app_state->spectrum_state);
    }
}

void app_platform_init(app_state_t *app_state)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);
    SetWindowIcon(LoadImage("assets/icon.png"));

    app_state->main_font = LoadFontEx("assets/fonts/retro-pixel-arcade.ttf", 64, 0, 250);
    app_state->windowed_w = WINDOW_WIDTH;
    app_state->windowed_h = WINDOW_HEIGHT;
}

int app_load_audio_data(app_state_t *app_state, const char *input_file)
{
    app_state->wave = LoadWave(input_file);
    if (app_state->wave.frameCount == 0)
    {
        fprintf(stderr, "Failed to load WAV: %s\n", input_file);
        app_cleanup(app_state);
        return 1;
    }

    app_state->sound = LoadSound(input_file);
    if (app_state->sound.frameCount == 0)
    {
        fprintf(stderr, "Failed to load sound: %s\n", input_file);
        app_cleanup(app_state);
        return 1;
    }

    app_state->samples = LoadWaveSamples(app_state->wave);
    if (!app_state->samples)
    {
        fprintf(stderr, "Failed to load wave samples: %s\n", input_file);
        app_cleanup(app_state);
        return 1;
    }

    return 0;
}

void app_run(app_state_t *app_state)
{
    PlaySound(app_state->sound);

    while (!WindowShouldClose())
    {
        double dt = GetFrameTime();

        app_handle_input(app_state);

        spectrum_handle_resize(&app_state->spectrum_state);
        spectrum_update(&app_state->spectrum_state, &app_state->wave, app_state->samples, dt);
        spectrum_render_to_texture(&app_state->spectrum_state);

        if (!app_state->loop_flag && spectrum_done(&app_state->spectrum_state))
        {
            break;
        }
        if (app_state->loop_flag && spectrum_done(&app_state->spectrum_state))
        {
            app_state->spectrum_state.window_index = 0;
            app_state->spectrum_state.accumulator = 0.0;
        }
        if (app_state->loop_flag && !IsSoundPlaying(app_state->sound))
        {
            PlaySound(app_state->sound);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        render_draw(&app_state->spectrum_state);
        EndDrawing();
    }
}

void app_cleanup(app_state_t *app_state)
{
    if (app_state->main_font.texture.id != 0)
    {
        UnloadFont(app_state->main_font);
    }

    if (app_state->samples)
    {
        UnloadWaveSamples(app_state->samples);
    }

    if (app_state->wave.data)
    {
        UnloadWave(app_state->wave);
    }

    if (app_state->sound.stream.buffer)
    {
        StopSound(app_state->sound);
        UnloadSound(app_state->sound);
    }

    spectrum_destroy(&app_state->spectrum_state);
    CloseAudioDevice();
    CloseWindow();

    if (app_state)
    {
        free(app_state);
    }
}
