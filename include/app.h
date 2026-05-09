#ifndef APP_H
#define APP_H

#include <raylib.h>
#include <portaudio.h>
#include <pthread.h>

#include "redefines.h"
#include "spectrum.h"

typedef struct
{
    i32 running;
    i32 freeze_enabled;
    i32 cursor_lock_enabled;
    i32 cursor_locked_index;
    i32 cursor_hover_index;
    i32 windowed_w;
    i32 windowed_h;
    i32 loop_flag;
    i32 fractional_octave_index_selected;

    Font main_font;

    spectrum_state_t spectrum_state;

    Wave wave;
    Music music;
    f32 *samples;
    f64 playback_time_prev;

    PaDeviceInfo *selected_device_info;
    PaStream *selected_device_stream;
    f64 input_sample_rate;

    // Live mic mode
    i32 mic_mode;               // 1=use mic input, 0=use WAV file
    f32 *mic_ring;              // mono ring buffer for captured samples
    ul mic_ring_capacity;       // capacity in frames
    volatile ul mic_ring_write; // producer index (callback)
    volatile ul mic_ring_read;  // consumer index (main thread)
    ul mic_ring_dropped_frames;
    pthread_mutex_t mic_ring_mutex;
    i32 mic_ring_mutex_initialized;
    f32 mic_window[FFT_WINDOW_SIZE]; // sliding window buffer for FFT
} app_state_t;

void
app_parse_input_args(i32 argc, char **argv, char **input_file, i32 *loop_flag, i32 *mic_mode);

void
app_handle_input(app_state_t *app_state);

i32
app_init_audio_capture(app_state_t *app_state);

i32
app_platform_init(app_state_t *app_state);

i32
app_load_audio_data(app_state_t *app_state, const char *input_file);

void
app_run(app_state_t *app_state);

void
app_cleanup(app_state_t *app_state);

#endif // APP_H
