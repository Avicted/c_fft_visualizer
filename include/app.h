#ifndef APP_H
#define APP_H

#include <raylib.h>
#include <portaudio.h>

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

    PaDeviceInfo *selected_device_info;
    PaStream *selected_device_stream;

    // Live mic mode
    i32 mic_mode;                    // 1=use mic input, 0=use WAV file
    f32 *mic_ring;                   // mono ring buffer for captured samples
    ul mic_ring_capacity;            // capacity in frames
    volatile ul mic_ring_write;      // producer index (callback)
    volatile ul mic_ring_read;       // consumer index (main thread)
    f32 mic_window[FFT_WINDOW_SIZE]; // sliding window buffer for FFT
} app_state_t;

void app_parse_input_args(i32 argc, char **argv, char **input_file, i32 *loop_flag, i32 *mic_mode);
void app_handle_input(app_state_t *app_state);
i32 app_init_audio_capture(app_state_t *app_state);
i32 app_platform_init(app_state_t *app_state);
i32 app_load_audio_data(app_state_t *app_state, const char *input_file);
void app_run(app_state_t *app_state);
void app_cleanup(app_state_t *app_state);

#endif
