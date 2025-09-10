#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "spectrum.h"

static int
calc_num_bars_for_width(int w)
{
    if (w <= 0)
    {
        return 0;
    }

    return w / BAR_PIXEL_WIDTH;
}

static void
update_plot_rect(spectrum_state *s)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    s->plot_left = MARGIN_LEFT;
    s->plot_top = MARGIN_TOP;
    s->plot_width = sw - (MARGIN_LEFT + MARGIN_RIGHT);
    s->plot_height = sh - (MARGIN_TOP + MARGIN_BOTTOM);
    if (s->plot_width < 10)
    {
        s->plot_width = 10;
    }
    if (s->plot_height < 10)
    {
        s->plot_height = 10;
    }
}

static Texture2D
create_gradient_texture(int h)
{
    Image img = GenImageGradientLinear(1, h, 0, BAR_GRADIENT_TOP_COLOR, BAR_GRADIENT_BOTTOM_COLOR);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

static int
allocate_bars(spectrum_state *s, int num)
{
    s->bar_target = (double *)calloc(num, sizeof(double));
    s->bar_smoothed = (double *)calloc(num, sizeof(double));
    s->peak_power = (double *)calloc(num, sizeof(double));
    s->bar_freq_center = (double *)calloc(num, sizeof(double));
    if (!s->bar_target || !s->bar_smoothed || !s->peak_power || !s->bar_freq_center)
    {
        free(s->bar_target);
        free(s->bar_smoothed);
        free(s->peak_power);
        free(s->bar_freq_center);
        s->bar_target = s->bar_smoothed = s->peak_power = s->bar_freq_center = NULL;
        return 0;
    }
    for (int b = 0; b < num; b++)
    {
        double t = (num > 1) ? (double)b / (double)(num - 1) : 0.0;
        s->bar_freq_center[b] = s->f_min * exp(s->log_f_ratio * t);
    }

    s->num_bars = num;
    return 1;
}

static void
free_bars(spectrum_state *s)
{
    free(s->bar_target);
    free(s->bar_smoothed);
    free(s->peak_power);
    free(s->bar_freq_center);
    s->bar_target = s->bar_smoothed = s->peak_power = s->bar_freq_center = NULL;
    s->num_bars = 0;
}

static int
reallocate_bars_if_needed(spectrum_state *s)
{
    int new_num = calc_num_bars_for_width(s->plot_width);
    if (new_num < 2)
    {
        new_num = 2;
    }
    if (new_num == s->num_bars)
    {
        return 1;
    }

    double *new_target = (double *)calloc(new_num, sizeof(double));
    double *new_smoothed = (double *)calloc(new_num, sizeof(double));
    double *new_peak = (double *)calloc(new_num, sizeof(double));
    double *new_freq_center = (double *)calloc(new_num, sizeof(double));
    if (!new_target || !new_smoothed || !new_peak || !new_freq_center)
    {
        free(new_target);
        free(new_smoothed);
        free(new_peak);
        free(new_freq_center);
        return 0;
    }
    for (int i = 0; i < new_num; i++)
    {
        double t = (double)i / (double)(new_num - 1);
        new_freq_center[i] = s->f_min * exp(s->log_f_ratio * t);
        if (s->num_bars > 0)
        {
            int old_index = (int)round(t * (s->num_bars - 1));
            if (old_index < 0)
            {
                old_index = 0;
            }
            if (old_index >= s->num_bars)
            {
                old_index = s->num_bars - 1;
            }

            new_target[i] = s->bar_target[old_index];
            new_smoothed[i] = s->bar_smoothed[old_index];
            new_peak[i] = s->peak_power[old_index];
        }
    }

    free_bars(s);
    s->bar_target = new_target;
    s->bar_smoothed = new_smoothed;
    s->peak_power = new_peak;
    s->bar_freq_center = new_freq_center;
    s->num_bars = new_num;

    return 1;
}

void spectrum_init(spectrum_state *s, Wave *wave, Font font)
{
    memset(s, 0, sizeof(*s));
    s->fft_bins = FFT_WINDOW_SIZE / 2 + 1;
    s->f_min = 20.0;
    s->f_max = wave->sampleRate / 2.0;
    s->log_f_ratio = log(s->f_max / s->f_min);
    s->fractional_k = pow(2.0, FRACTIONAL_OCTAVE / 2.0);
    s->seconds_per_window = (double)FFT_WINDOW_SIZE / (double)wave->sampleRate;
    s->font = font;
    update_plot_rect(s);
    s->gradient_tex = create_gradient_texture(s->plot_height);
    s->fft_rt = LoadRenderTexture(s->plot_width, s->plot_height);
    allocate_bars(s, calc_num_bars_for_width(s->plot_width));
    s->fft_plan = fftw_plan_dft_r2c_1d(FFT_WINDOW_SIZE, s->fft_in, s->fft_out, FFTW_ESTIMATE);
    s->last_width = GetScreenWidth();
    s->last_height = GetScreenHeight();
}

void spectrum_destroy(spectrum_state *s)
{
    if (s->gradient_tex.id)
    {
        UnloadTexture(s->gradient_tex);
    }
    if (s->fft_rt.id)
    {
        UnloadRenderTexture(s->fft_rt);
    }
    free_bars(s);
    if (s->fft_plan)
    {
        fftw_destroy_plan(s->fft_plan);
    }
}

void spectrum_set_total_windows(spectrum_state *s, int total)
{
    s->total_windows = total;
    s->window_index = 0;
    s->accumulator = 0.0;
}

int spectrum_done(const spectrum_state *s)
{
    return s->window_index >= s->total_windows;
}

void spectrum_handle_resize(spectrum_state *s)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (sw == s->last_width && sh == s->last_height)
    {
        return;
    }

    s->last_width = sw;
    s->last_height = sh;
    update_plot_rect(s);
    if (s->fft_rt.id)
    {
        UnloadRenderTexture(s->fft_rt);
        s->fft_rt = LoadRenderTexture(s->plot_width, s->plot_height);
    }
    if (s->gradient_tex.id)
    {
        UnloadTexture(s->gradient_tex);
        s->gradient_tex = create_gradient_texture(s->plot_height);
    }

    reallocate_bars_if_needed(s);
}

static void
compute_fft_window(spectrum_state *s, float *samples, Wave *wave)
{
    int start_index = s->window_index * FFT_WINDOW_SIZE * wave->channels;
    for (int i = 0; i < FFT_WINDOW_SIZE; i++)
    {
        float mono;
        if (wave->channels == 1)
        {
            mono = samples[start_index + i];
        }
        else
        {
            mono = 0.5f * (samples[start_index + (i * 2)] + samples[start_index + (i * 2) + 1]);
        }

        s->fft_in[i] = mono;
    }

    fftw_execute(s->fft_plan);
    for (int i = 0; i < s->fft_bins; i++)
    {
        double re = s->fft_out[i][0];
        double im = s->fft_out[i][1];
        s->bin_mag[i] = sqrt(re * re + im * im) / FFT_WINDOW_SIZE;
    }
}

static void
compute_bar_targets(spectrum_state *s, int sample_rate)
{
    for (int b = 0; b < s->num_bars; b++)
    {
        double f_center = s->bar_freq_center[b];
        double f_low = f_center / s->fractional_k;
        double f_high = f_center * s->fractional_k;
        if (f_low < s->f_min)
        {
            f_low = s->f_min;
        }
        if (f_high > s->f_max)
        {
            f_high = s->f_max;
        }

        int bin_low = (int)ceil(f_low * FFT_WINDOW_SIZE / (double)sample_rate);
        int bin_high = (int)floor(f_high * FFT_WINDOW_SIZE / (double)sample_rate);
        if (bin_low < 1)
        {
            bin_low = 1;
        }
        if (bin_high >= s->fft_bins)
        {
            bin_high = s->fft_bins - 1;
        }
        if (bin_high < bin_low)
        {
            bin_high = bin_low;
        }

        double power_sum = 0.0;
        int count = 0;
        for (int kbin = bin_low; kbin <= bin_high; kbin++)
        {
            double a = s->bin_mag[kbin];
            power_sum += a * a;
            count++;
        }

        s->bar_target[b] = count ? (power_sum / (double)count) : 0.0;
    }
}

static void
smooth_bars(spectrum_state *s)
{
    for (int b = 0; b < s->num_bars; b++)
    {
        s->bar_smoothed[b] += LERP_SPEED * (s->bar_target[b] - s->bar_smoothed[b]);
    }
}

static void
update_peaks(spectrum_state *s, double dt)
{
    double decay_factor = pow(10.0, -PEAK_DECAY_DB_PER_SEC * dt / 10.0);
    for (int b = 0; b < s->num_bars; b++)
    {
        if (s->bar_smoothed[b] > s->peak_power[b])
        {
            s->peak_power[b] = s->bar_smoothed[b];
        }
        else
        {
            s->peak_power[b] *= decay_factor;
            if (s->peak_power[b] < s->bar_smoothed[b])
            {
                s->peak_power[b] = s->bar_smoothed[b];
            }
        }
    }
}

void spectrum_update(spectrum_state *s, Wave *wave, float *samples, double dt)
{
    if (spectrum_done(s))
    {
        return;
    }

    s->accumulator += dt;
    if (s->accumulator >= s->seconds_per_window)
    {
        s->accumulator -= s->seconds_per_window;
        compute_fft_window(s, samples, wave);
        compute_bar_targets(s, wave->sampleRate);
        s->window_index++;
    }

    smooth_bars(s);
    update_peaks(s, dt);
}

void spectrum_render_to_texture(spectrum_state *s)
{
    BeginTextureMode(s->fft_rt);
    ClearBackground(BLACK);
    int h = s->plot_height;
    for (int b = 0; b < s->num_bars; b++)
    {
        double mag_db = 10.0 * log10(s->bar_smoothed[b] + EPSILON_POWER);
        if (mag_db < DB_BOTTOM)
        {
            mag_db = DB_BOTTOM;
        }
        if (mag_db > DB_TOP)
        {
            mag_db = DB_TOP;
        }

        double norm = (mag_db - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        int bar_h = (int)(norm * h);
        if (bar_h <= 0)
        {
            continue;
        }

        int x = b * BAR_PIXEL_WIDTH;
        Rectangle src = {0, (float)(s->gradient_tex.height - bar_h), 1, (float)bar_h};
        Rectangle dst = {(float)x, (float)(h - bar_h), (float)BAR_PIXEL_WIDTH, (float)bar_h};
        DrawTexturePro(s->gradient_tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }

    Color peak_color = (Color){BAR_GRADIENT_TOP_COLOR.r, BAR_GRADIENT_TOP_COLOR.g, BAR_GRADIENT_TOP_COLOR.b, 200};
    for (int b = 0; b < s->num_bars; b++)
    {
        double p = s->peak_power[b];
        if (p <= 0)
        {
            continue;
        }

        double peak_db = 10.0 * log10(p + EPSILON_POWER);
        if (peak_db < DB_BOTTOM)
        {
            continue;
        }
        if (peak_db > DB_TOP)
        {
            peak_db = DB_TOP;
        }

        double norm = (peak_db - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        int y = h - (int)(norm * h);
        DrawRectangle(b * BAR_PIXEL_WIDTH, y, BAR_PIXEL_WIDTH, 1, peak_color);
    }

    EndTextureMode();
}
