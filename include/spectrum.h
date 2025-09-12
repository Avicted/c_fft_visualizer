#ifndef SPECTRUM_H
#define SPECTRUM_H

#include "redefines.h"

#include <raylib.h>
#include <fftw3.h>
#include "config.h"

#define FRACTIONAL_OCTAVE_1_1 1
#define FRACTIONAL_OCTAVE_1_3 (1.0 / 3.0)
#define FRACTIONAL_OCTAVE_1_6 (1.0 / 6.0)
#define FRACTIONAL_OCTAVE_1_12 (1.0 / 12.0)
#define FRACTIONAL_OCTAVE_1_24 (1.0 / 24.0)
#define FRACTIONAL_OCTAVE_1_48 (1.0 / 48.0)

#define NUM_FRACTIONAL_OCTAVES 6

extern const f64 FRACTIONAL_OCTAVES[NUM_FRACTIONAL_OCTAVES];

#define NUM_BAR_GRADIENTS 3

typedef struct
{
    Color bottom;
    Color top;
} bar_gradient_t;

typedef struct spectrum_state_t
{
    i32 bar_gradient_index;
    bar_gradient_t bar_gradients[NUM_BAR_GRADIENTS];

    f64 fft_in[FFT_WINDOW_SIZE];
    fftw_complex fft_out[FFT_WINDOW_SIZE / 2 + 1];
    fftw_plan fft_plan;
    i32 fft_bins;
    f64 bin_mag[FFT_WINDOW_SIZE / 2 + 1];

    i32 num_bars;
    i32 sample_rate;
    f64 *bar_target;
    f64 *bar_smoothed;
    f64 *peak_power;
    f64 *bar_freq_center;

    f64 f_min;
    f64 f_max;
    f64 log_f_ratio;

    f64 fractional_octave;
    f64 fractional_k;
    i32 fractional_octave_index;

    f64 seconds_per_window;
    f64 accumulator;

    i32 hop_size;
    f64 window[FFT_WINDOW_SIZE];
    f64 hpf_alpha;
    f64 hpf_prev_x;
    f64 hpf_prev_y;

    i32 window_index;
    i32 total_windows;
    Texture2D gradient_tex;
    RenderTexture2D fft_rt;
    i32 last_width;
    i32 last_height;
    i32 plot_left;
    i32 plot_top;
    i32 plot_width;
    i32 plot_height;
    Font font;

    f64 meter_interval_elapsed;
    f64 meter_sum_sq;
    f64 meter_peak_lin;
    i32 meter_sample_count;
    f64 meter_rms_dbfs;
    f64 meter_peak_dbfs;
} spectrum_state_t;

void spectrum_set_fractional_octave(spectrum_state_t *s, f64 frac, i32 index);
Texture2D create_gradient_texture(i32 height, bar_gradient_t grad);

void spectrum_init(spectrum_state_t *s, Wave *wave, Font font);
void spectrum_destroy(spectrum_state_t *s);
void spectrum_set_total_windows(spectrum_state_t *s, i32 total);
i32 spectrum_done(const spectrum_state_t *s);
void spectrum_handle_resize(spectrum_state_t *s);
void spectrum_update(spectrum_state_t *s, Wave *wave, f32 *samples, f64 dt);
void spectrum_render_to_texture(spectrum_state_t *s);

#endif
