#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

#include "app.c"
#include "spectrum.c"
#include "render.c"

int main(int argc, char **argv)
{
    app_state_t *app_state = (app_state_t *)calloc(1, sizeof(app_state_t));
    if (!app_state)
    {
        fprintf(stderr, "Failed to allocate application state\n");
        return 1;
    }

    const char *input_file = NULL;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--loop") == 0 || strcmp(argv[i], "-l") == 0)
        {
            app_state->loop_flag = 1;
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

    app_platform_init();

    app_state->main_font = LoadFontEx("assets/fonts/retro-pixel-arcade.ttf", 64, 0, 250);
    app_state->windowed_w = WINDOW_WIDTH;
    app_state->windowed_h = WINDOW_HEIGHT;

    app_state->wave = LoadWave(input_file);
    if (app_state->wave.frameCount == 0)
    {
        fprintf(stderr, "Failed to load WAV: %s\n", input_file);
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    app_state->sound = LoadSound(input_file);
    if (app_state->sound.frameCount == 0)
    {
        fprintf(stderr, "Failed to load sound: %s\n", input_file);
        UnloadWave(app_state->wave);
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    PlaySound(app_state->sound);
    app_state->samples = LoadWaveSamples(app_state->wave);
    if (!app_state->samples)
    {
        fprintf(stderr, "Failed to load wave samples: %s\n", input_file);
        UnloadSound(app_state->sound);
        UnloadWave(app_state->wave);
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    spectrum_init(&app_state->spectrum_state, &app_state->wave, app_state->main_font);
    spectrum_set_total_windows(&app_state->spectrum_state, app_state->wave.frameCount / FFT_WINDOW_SIZE);

    app_run(app_state);

    app_cleanup(app_state);

    return 0;
}
