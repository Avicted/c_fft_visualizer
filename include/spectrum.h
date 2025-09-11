#ifndef SPECTRUM_H
#define SPECTRUM_H

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

extern const double FRACTIONAL_OCTAVES[NUM_FRACTIONAL_OCTAVES];

#define NUM_BAR_GRADIENTS 3

typedef struct
{
    Color bottom;
    Color top;
} bar_gradient_t;

typedef struct spectrum_state_t
{
    int bar_gradient_index;
    bar_gradient_t bar_gradients[NUM_BAR_GRADIENTS];

    double fft_in[FFT_WINDOW_SIZE];
    fftw_complex fft_out[FFT_WINDOW_SIZE / 2 + 1];
    fftw_plan fft_plan;
    int fft_bins;
    double bin_mag[FFT_WINDOW_SIZE / 2 + 1];

    int num_bars;
    int sample_rate;
    double *bar_target;
    double *bar_smoothed;
    double *peak_power;
    double *bar_freq_center;

    double f_min;
    double f_max;
    double log_f_ratio;

    double fractional_octave;
    double fractional_k;
    int fractional_octave_index;

    double seconds_per_window;
    double accumulator;

    int hop_size;
    double window[FFT_WINDOW_SIZE];
    double hpf_alpha;
    double hpf_prev_x;
    double hpf_prev_y;

    int window_index;
    int total_windows;
    Texture2D gradient_tex;
    RenderTexture2D fft_rt;
    int last_width;
    int last_height;
    int plot_left;
    int plot_top;
    int plot_width;
    int plot_height;
    Font font;

    double meter_interval_elapsed;
    double meter_sum_sq;
    double meter_peak_lin;
    int meter_sample_count;
    double meter_rms_dbfs;
    double meter_peak_dbfs;
} spectrum_state_t;

void spectrum_set_fractional_octave(spectrum_state_t *s, double frac, int index);
Texture2D create_gradient_texture(int height, bar_gradient_t grad);

void spectrum_init(spectrum_state_t *s, Wave *wave, Font font);
void spectrum_destroy(spectrum_state_t *s);
void spectrum_set_total_windows(spectrum_state_t *s, int total);
int spectrum_done(const spectrum_state_t *s);
void spectrum_handle_resize(spectrum_state_t *s);
void spectrum_update(spectrum_state_t *s, Wave *wave, float *samples, double dt);
void spectrum_render_to_texture(spectrum_state_t *s);

#endif
