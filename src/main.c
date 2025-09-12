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
    app_parse_input_args(argc, argv, (char **)&input_file, &app_state->loop_flag);

    app_platform_init(app_state);

    i32 load_result = app_load_audio_data(app_state, input_file);
    if (load_result != 0)
    {
        app_cleanup(app_state);
        return load_result;
    }

    spectrum_init(&app_state->spectrum_state, &app_state->wave, app_state->main_font);
    {
        i32 index = app_state->fractional_octave_index_selected;
        f64 frac = FRACTIONAL_OCTAVES[index];
        spectrum_set_fractional_octave(&app_state->spectrum_state, frac, index);

        i32 hop = FFT_HOP_SIZE;
        long frames = (long)app_state->wave.frameCount;
        long total = (frames > FFT_WINDOW_SIZE) ? (1 + (frames - FFT_WINDOW_SIZE) / hop) : 1;
        if (total < 1)
        {
            total = 1;
        }

        spectrum_set_total_windows(&app_state->spectrum_state, (i32)total);
    }

    app_run(app_state);

    app_cleanup(app_state);

    return 0;
}
