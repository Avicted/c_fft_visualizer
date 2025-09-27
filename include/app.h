#ifndef APP_H
#define APP_H

#include <raylib.h>

#include "redefines.h"
#include "spectrum.h"
#include "render.h"

typedef struct
{
    i32 running;
    i32 windowed_w;
    i32 windowed_h;
    i32 loop_flag;
    i32 fractional_octave_index_selected;

    Font main_font;

    spectrum_state_t spectrum_state;

    Wave wave;
    Sound sound;
    f32 *samples;
} app_state_t;

void app_parse_input_args(i32 argc, char **argv, char **input_file, i32 *loop_flag);
void app_handle_input(app_state_t *app_state);
void app_platform_init(app_state_t *app_state);
i32 app_load_audio_data(app_state_t *app_state, const char *input_file);
void app_run(app_state_t *app_state);
void app_cleanup(app_state_t *app_state);

#endif
