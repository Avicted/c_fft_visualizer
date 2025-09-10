#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>

#include "spectrum.c"
#include "render.c"

static const char *
read_input_file_path(int argc, char **argv)
{
    if (argc >= 2)
    {
        return argv[1];
    }

    return NULL;
}

int main(int argc, char **argv)
{
    const char *input_file = read_input_file_path(argc, argv);
    if (!input_file)
    {
        fprintf(stderr, "Usage: %s <wav file>\n", argv[0]);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);

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

    while (!WindowShouldClose() && !spectrum_done(&state))
    {
        double dt = GetFrameTime();
        spectrum_handle_resize(&state);
        spectrum_update(&state, &wave, samples, dt);
        spectrum_render_to_texture(&state);
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
