#ifndef APP_H
#define APP_H

#include <raylib.h>

#include "spectrum.h"
#include "render.h"

typedef struct app_state_t
{
    int is_running;
    int is_fullscreen;
    int windowed_w;
    int windowed_h;
    int loop_flag;

    Font main_font;

    spectrum_state_t spectrum_state;

    Wave wave;
    Sound sound;
    float *samples;
} app_state_t;

void app_handle_input(app_state_t *app_state);
void app_platform_init(void);
void app_run(app_state_t *app_state);
void app_cleanup(app_state_t *app_state);

#endif
