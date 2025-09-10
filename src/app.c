#include "app.h"

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

void app_platform_init(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);
    SetWindowIcon(LoadImage("assets/icon.png"));
}

void app_run(app_state_t *app_state)
{
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
    UnloadFont(app_state->main_font);
    UnloadWaveSamples(app_state->samples);
    UnloadWave(app_state->wave);
    StopSound(app_state->sound);
    UnloadSound(app_state->sound);
    spectrum_destroy(&app_state->spectrum_state);
    CloseAudioDevice();
    CloseWindow();

    free(app_state);
}
