#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raylib.h>

#include "app.c"
#include "spectrum.c"
#include "render.c"

i32 main(i32 argc, char **argv)
{
    app_state_t *app_state = (app_state_t *)calloc(1, sizeof(app_state_t));
    if (!app_state)
    {
        fprintf(stderr, "Failed to allocate application state\n");
        return 1;
    }

    const char *input_file = NULL;
    app_parse_input_args(argc, argv, (char **)&input_file, &app_state->loop_flag, &app_state->mic_mode);

    if (app_platform_init(app_state) != 0)
    {
        app_cleanup(app_state);
        return 1;
    }

    if (!app_state->mic_mode)
    {
        if (app_load_audio_data(app_state, input_file) != 0)
        {
            app_cleanup(app_state);
            return 1;
        }

        spectrum_init(&app_state->spectrum_state, &app_state->wave, app_state->main_font);
        {
            i32 index = app_state->fractional_octave_index_selected;
            f64 frac = FRACTIONAL_OCTAVES[index];
            spectrum_set_fractional_octave(&app_state->spectrum_state, frac, index);

            i32 hop = FFT_HOP_SIZE;
            ul frames = (ul)app_state->wave.frameCount;
            ul total = (frames > FFT_WINDOW_SIZE) ? (1 + (frames - FFT_WINDOW_SIZE) / hop) : 1;
            if (total < 1)
            {
                total = 1;
            }

            spectrum_set_total_windows(&app_state->spectrum_state, (i32)total);
        }
    }
    else
    {
        Wave live_wave = {0};
        live_wave.channels = 1;
        live_wave.sampleRate = INPUT_SAMPLE_RATE;
        live_wave.frameCount = FFT_WINDOW_SIZE;
        spectrum_init(&app_state->spectrum_state, &live_wave, app_state->main_font);

        i32 index = app_state->fractional_octave_index_selected;
        f64 frac = FRACTIONAL_OCTAVES[index];
        spectrum_set_fractional_octave(&app_state->spectrum_state, frac, index);

        spectrum_set_total_windows(&app_state->spectrum_state, 1);

        if (app_init_audio_capture(app_state) != 0)
        {
            fprintf(stderr, "ERROR: Failed to initialize audio capture\n");
            app_cleanup(app_state);
            return 1;
        }
    }

    app_run(app_state);

    app_cleanup(app_state);

    return 0;
}
