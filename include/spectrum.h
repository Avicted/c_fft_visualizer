#ifndef SPECTRUM_H
#define SPECTRUM_H

#include <raylib.h>
#include <fftw3.h>
#include "config.h"

typedef struct spectrum_state_t
{
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
    double fractional_k;
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
} spectrum_state_t;

void spectrum_init(spectrum_state_t *s, Wave *wave, Font font);
void spectrum_destroy(spectrum_state_t *s);
void spectrum_set_total_windows(spectrum_state_t *s, int total);
int spectrum_done(const spectrum_state_t *s);
void spectrum_handle_resize(spectrum_state_t *s);
void spectrum_update(spectrum_state_t *s, Wave *wave, float *samples, double dt);
void spectrum_render_to_texture(spectrum_state_t *s);

#endif
