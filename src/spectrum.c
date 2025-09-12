#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "spectrum.h"

static void compute_bar_targets(spectrum_state_t *s);

const f64 FRACTIONAL_OCTAVES[NUM_FRACTIONAL_OCTAVES] = {
    1.0,
    1.0 / 3.0,
    1.0 / 6.0,
    1.0 / 12.0,
    1.0 / 24.0,
    1.0 / 48.0,
};

void spectrum_set_fractional_octave(spectrum_state_t *s, f64 frac, i32 index)
{
    if (frac <= 0.0)
    {
        frac = 1.0 / 24.0;
        index = 4;
    }

    s->fractional_octave = frac;
    s->fractional_k = pow(2.0, frac / 2.0);
    s->fractional_octave_index = index;
}

Texture2D
create_gradient_texture(i32 height, bar_gradient_t grad)
{
    Image img = GenImageGradientLinear(1, height, 0, grad.top, grad.bottom);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    return tex;
}

static int
calc_num_bars_for_width(i32 w)
{
    if (w <= 0)
    {
        return 0;
    }

    return w / (BAR_PIXEL_WIDTH + BAR_GAP);
}

static void
update_plot_rect(spectrum_state_t *s)
{
    i32 sw = GetScreenWidth();
    i32 sh = GetScreenHeight();
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

static int
allocate_bars(spectrum_state_t *s, i32 num)
{
    s->bar_target = (f64 *)calloc(num, sizeof(f64));
    s->bar_smoothed = (f64 *)calloc(num, sizeof(f64));
    s->peak_power = (f64 *)calloc(num, sizeof(f64));
    s->bar_freq_center = (f64 *)calloc(num, sizeof(f64));
    if (!s->bar_target || !s->bar_smoothed || !s->peak_power || !s->bar_freq_center)
    {
        free(s->bar_target);
        free(s->bar_smoothed);
        free(s->peak_power);
        free(s->bar_freq_center);
        s->bar_target = s->bar_smoothed = s->peak_power = s->bar_freq_center = NULL;
        return 0;
    }

    for (i32 b = 0; b < num; b++)
    {
        f64 t = (f64)b / (f64)(num - 1);
        s->bar_freq_center[b] = s->f_min * pow(s->f_max / s->f_min, t);
    }

    s->num_bars = num;
    return 1;
}

static void
free_bars(spectrum_state_t *s)
{
    free(s->bar_target);
    free(s->bar_smoothed);
    free(s->peak_power);
    free(s->bar_freq_center);
    s->bar_target = s->bar_smoothed = s->peak_power = s->bar_freq_center = NULL;
    s->num_bars = 0;
}

static int
reallocate_bars_if_needed(spectrum_state_t *s)
{
    i32 new_num = calc_num_bars_for_width(s->plot_width);
    if (new_num < 2)
    {
        new_num = 2;
    }
    if (new_num == s->num_bars)
    {
        return 1;
    }

    f64 *new_target = (f64 *)calloc(new_num, sizeof(f64));
    f64 *new_smoothed = (f64 *)calloc(new_num, sizeof(f64));
    f64 *new_peak = (f64 *)calloc(new_num, sizeof(f64));
    f64 *new_freq_center = (f64 *)calloc(new_num, sizeof(f64));
    if (!new_target || !new_smoothed || !new_peak || !new_freq_center)
    {
        free(new_target);
        free(new_smoothed);
        free(new_peak);
        free(new_freq_center);
        return 0;
    }

    for (i32 i = 0; i < new_num; i++)
    {
        f64 t = (f64)i / (f64)(new_num - 1);

        new_freq_center[i] = s->f_min * pow(s->f_max / s->f_min, t);

        if (s->num_bars > 0)
        {
            f64 f = new_freq_center[i];
            i32 closest_index = 0;
            f64 min_dist = INFINITY;

            for (i32 j = 0; j < s->num_bars; j++)
            {
                f64 dist = fabs(log(f / s->bar_freq_center[j]));
                if (dist < min_dist)
                {
                    min_dist = dist;
                    closest_index = j;
                }
            }

            new_target[i] = s->bar_target[closest_index];
            new_smoothed[i] = s->bar_smoothed[closest_index];
            new_peak[i] = s->peak_power[closest_index];
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

void spectrum_init(spectrum_state_t *s, Wave *wave, Font font)
{
    memset(s, 0, sizeof(*s));

    s->bar_gradients[0] = (bar_gradient_t){(Color){255, 128, 0, 255}, (Color){255, 255, 0, 255}};
    s->bar_gradients[1] = (bar_gradient_t){(Color){0, 32, 255, 255}, (Color){0, 255, 255, 255}};
    s->bar_gradients[2] = (bar_gradient_t){(Color){0, 255, 0, 255}, (Color){0, 255, 255, 255}};
    s->bar_gradient_index = 2;

    s->fft_bins = FFT_WINDOW_SIZE / 2 + 1;
    s->f_min = 20.0;
    s->f_max = fmin(20000.0, (f64)wave->sampleRate * 0.5);
    s->log_f_ratio = log(s->f_max / s->f_min);

    s->fractional_octave_index = 4;
    s->fractional_octave = FRACTIONAL_OCTAVES[s->fractional_octave_index];
    s->fractional_k = pow(2.0, s->fractional_octave / 2.0);
    s->hop_size = FFT_HOP_SIZE;
    s->seconds_per_window = (f64)s->hop_size / (f64)wave->sampleRate;
    s->font = font;
    s->sample_rate = wave->sampleRate;

    for (i32 i = 0; i < FFT_WINDOW_SIZE; i++)
    {
        s->window[i] = 0.5 * (1.0 - cos((2.0 * PI * i) / (f64)(FFT_WINDOW_SIZE - 1)));
    }

    f64 rc = 1.0 / (2.0 * PI * HPF_CUTOFF_HZ);
    f64 dt = 1.0 / (f64)wave->sampleRate;
    s->hpf_alpha = rc / (rc + dt);
    s->hpf_prev_x = 0.0;
    s->hpf_prev_y = 0.0;

    update_plot_rect(s);
    s->gradient_tex = create_gradient_texture(s->plot_height, s->bar_gradients[s->bar_gradient_index]);
    s->fft_rt = LoadRenderTexture(s->plot_width, s->plot_height);
    allocate_bars(s, calc_num_bars_for_width(s->plot_width));
    s->fft_plan = fftw_plan_dft_r2c_1d(FFT_WINDOW_SIZE, s->fft_in, s->fft_out, FFTW_ESTIMATE);
    s->last_width = GetScreenWidth();
    s->last_height = GetScreenHeight();

    s->meter_interval_elapsed = 0.0;
    s->meter_sum_sq = 0.0;
    s->meter_peak_lin = 0.0;
    s->meter_sample_count = 0;
    s->meter_rms_dbfs = NAN;
    s->meter_peak_dbfs = NAN;
}

void spectrum_destroy(spectrum_state_t *s)
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

void spectrum_set_total_windows(spectrum_state_t *s, i32 total)
{
    s->total_windows = total;
    s->window_index = 0;
    s->accumulator = 0.0;
}

i32 spectrum_done(const spectrum_state_t *s)
{
    return s->window_index >= s->total_windows;
}

void spectrum_handle_resize(spectrum_state_t *s)
{
    i32 sw = GetScreenWidth();
    i32 sh = GetScreenHeight();
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
        s->gradient_tex = create_gradient_texture(s->plot_height, s->bar_gradients[s->bar_gradient_index]);
    }

    reallocate_bars_if_needed(s);
}

static void
compute_fft_window(spectrum_state_t *s, f32 *samples, Wave *wave)
{
    usize total_samples = (size_t)wave->frameCount * (size_t)wave->channels;
    usize start_index = (size_t)s->window_index * (size_t)s->hop_size * (size_t)wave->channels;

    f64 mean = 0.0;
    static f32 mono_buf[FFT_WINDOW_SIZE];

    for (i32 i = 0; i < FFT_WINDOW_SIZE; i++)
    {
        usize si = start_index + (size_t)i * (size_t)wave->channels;
        f32 mono = 0.0f;
        if (si < total_samples)
        {
            if (wave->channels == 1)
                mono = samples[si];
            else
            {
                f32 a = samples[si];
                f32 b = (si + 1 < total_samples) ? samples[si + 1] : 0.0f;
                mono = 0.5f * (a + b);
            }
        }
        mono_buf[i] = mono;
        mean += mono;

        // Meter accumulation (raw mono sample before mean removal / HPF / window)
        f64 absx = fabs((f64)mono);
        if (absx > s->meter_peak_lin)
        {
            s->meter_peak_lin = absx;
        }

        s->meter_sum_sq += (f64)mono * (f64)mono;
        s->meter_sample_count++;
    }
    mean /= (f64)FFT_WINDOW_SIZE;

    // Second pass: HPF + window
    for (i32 i = 0; i < FFT_WINDOW_SIZE; i++)
    {
        f64 x = (f64)mono_buf[i] - mean;
        f64 y = s->hpf_alpha * (s->hpf_prev_y + x - s->hpf_prev_x);
        s->hpf_prev_x = x;
        s->hpf_prev_y = y;
        s->fft_in[i] = y * s->window[i];
    }

    fftw_execute(s->fft_plan);

    // Correct single-sided spectrum scaling with Hann coherent gain
    // Hann coherent gain = 0.5 -> scale = 2/(N*0.5) = 4/N
    f64 scale = 4.0 / (f64)FFT_WINDOW_SIZE;
    for (i32 i = 0; i < s->fft_bins; i++)
    {
        f64 re = s->fft_out[i][0];
        f64 im = s->fft_out[i][1];
        f64 a = sqrt(re * re + im * im) * scale;
        if (i == 0 || i == s->fft_bins - 1)
        {
            a *= 0.5;
        }

        s->bin_mag[i] = a;
    }
}

static void
compute_bar_targets(spectrum_state_t *s)
{
    // Use Nyquist for Hz->bin mapping (independent of displayed f_max)
    f64 max_bin = (f64)(s->fft_bins - 1);
    f64 nyquist = (f64)s->sample_rate * 0.5;
    f64 hz_to_bin = max_bin / nyquist;

    for (i32 b = 0; b < s->num_bars; b++)
    {
        // Calculate logarithmically-spaced frequencies
        f64 t = (f64)b / (f64)(s->num_bars - 1);
        f64 f_center = s->f_min * pow(s->f_max / s->f_min, t);

        // Compute band edges using constant-Q factor
        f64 f_low = f_center / s->fractional_k;
        f64 f_high = f_center * s->fractional_k;

        if (f_low < s->f_min)
        {
            f_low = s->f_min;
        }
        if (f_high > s->f_max)
        {
            f_high = s->f_max;
        }

        // Convert to FFT bin indices
        f64 k_lo = f_low * hz_to_bin;
        f64 k_hi = f_high * hz_to_bin;

        // Ensure we don't miss the first bin (which can contain significant energy)
        if (b == 0)
        {
            k_lo = 0.0; // Include DC for first band
        }
        if (k_hi > max_bin)
        {
            k_hi = max_bin;
        }

        i32 k0 = (i32)floor(k_lo);
        i32 k1 = (i32)floor(k_hi);

        // Handle special case: no bins in range
        if (k_hi <= k_lo || k1 < k0)
        {
            s->bar_target[b] = 0.0;
            continue;
        }

        f64 sum = 0.0;
        f64 width = 0.0;

        // Handle single-bin case
        if (k0 == k1)
        {
            f64 w = k_hi - k_lo;
            if (w > 0.0)
            {
                f64 p = s->bin_mag[k0];
                sum += p * p * w;
                width += w;
            }
        }
        else // Multiple bins
        {
            // First bin (partial)
            f64 frac0 = 1.0 - (k_lo - floor(k_lo));
            if (frac0 > 0.0 && k0 >= 0 && k0 < s->fft_bins)
            {
                f64 p = s->bin_mag[k0];
                sum += p * p * frac0;
                width += frac0;
            }

            // Middle bins (full)
            for (i32 k = k0 + 1; k < k1; k++)
            {
                if (k >= 0 && k < s->fft_bins)
                {
                    f64 p = s->bin_mag[k];
                    sum += p * p;
                    width += 1.0;
                }
            }

            // Last bin (partial)
            f64 frac1 = k_hi - floor(k_hi);
            if (frac1 > 0.0 && k1 >= 0 && k1 < s->fft_bins)
            {
                f64 p = s->bin_mag[k1];
                sum += p * p * frac1;
                width += frac1;
            }
        }

        f64 avg_power = (width > 0.0) ? (sum / width) : 0.0;

        s->bar_target[b] = avg_power * (f_center / 1000.0);
    }
}

static void
smooth_bars(spectrum_state_t *s, f64 dt)
{
    f64 tau_a = SMOOTH_ATTACK_MS * 0.001;
    f64 tau_r = SMOOTH_RELEASE_MS * 0.001;
    f64 a_up = 1.0 - exp(-dt / tau_a);
    f64 a_dn = 1.0 - exp(-dt / tau_r);

    for (i32 b = 0; b < s->num_bars; b++)
    {
        f64 y = s->bar_smoothed[b];
        f64 x = s->bar_target[b];
        f64 a = (x > y) ? a_up : a_dn;
        s->bar_smoothed[b] = y + a * (x - y);
    }
}

static void
update_peaks(spectrum_state_t *s, f64 dt)
{
    f64 decay_factor = pow(10.0, -PEAK_DECAY_DB_PER_SEC * dt / 10.0);
    for (i32 b = 0; b < s->num_bars; b++)
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

void spectrum_update(spectrum_state_t *s, Wave *wave, f32 *samples, f64 dt)
{
    if (spectrum_done(s))
    {
        return;
    }

    s->accumulator += dt;
    while (s->accumulator >= s->seconds_per_window && !spectrum_done(s))
    {
        s->accumulator -= s->seconds_per_window;
        compute_fft_window(s, samples, wave);
        compute_bar_targets(s);
        s->window_index++;
    }

    const f64 meter_update_interval_seconds = 1.0;
    s->meter_interval_elapsed += dt;
    if (s->meter_interval_elapsed >= meter_update_interval_seconds)
    {
        if (s->meter_sample_count > 0)
        {
            f64 rms_lin = sqrt(s->meter_sum_sq / (f64)s->meter_sample_count);
            if (s->meter_peak_lin < 1e-12)
            {
                s->meter_peak_dbfs = -INFINITY;
            }
            else
            {
                s->meter_peak_dbfs = 20.0 * log10(s->meter_peak_lin);
            }

            if (rms_lin < 1e-12)
            {
                s->meter_rms_dbfs = -INFINITY;
            }
            else
            {
                s->meter_rms_dbfs = 20.0 * log10(rms_lin);
            }
        }
        else
        {
            s->meter_peak_dbfs = s->meter_rms_dbfs = NAN;
        }

        // reset accumulators, keep spill remainder
        s->meter_interval_elapsed = fmod(s->meter_interval_elapsed, 1.0);
        s->meter_sum_sq = 0.0;
        s->meter_peak_lin = 0.0;
        s->meter_sample_count = 0;
    }

    smooth_bars(s, dt);
    update_peaks(s, dt);
}

void spectrum_render_to_texture(spectrum_state_t *s)
{
    BeginTextureMode(s->fft_rt);
    ClearBackground(BLACK);
    i32 h = s->plot_height;

    const i32 stride = BAR_PIXEL_WIDTH + BAR_GAP;

    for (i32 b = 0; b < s->num_bars; b++)
    {
        f64 mag_db = 10.0 * log10(s->bar_smoothed[b] + EPSILON_POWER) + DB_OFFSET;
        if (mag_db < DB_BOTTOM)
        {
            mag_db = DB_BOTTOM;
        }
        if (mag_db > DB_TOP)
        {
            mag_db = DB_TOP;
        }

        f64 norm = (mag_db - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        i32 bar_h = (i32)(norm * h);
        if (bar_h <= 0)
        {
            continue;
        }

        i32 x = b * stride;

        Rectangle src = {0, (f32)(s->gradient_tex.height - bar_h), 1, (f32)bar_h};
        Rectangle dst = {(f32)x, (f32)(h - bar_h), (f32)BAR_PIXEL_WIDTH, (f32)bar_h};
        DrawTexturePro(s->gradient_tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }

    Color peak_color = (Color){
        s->bar_gradients[s->bar_gradient_index].top.r,
        s->bar_gradients[s->bar_gradient_index].top.g,
        s->bar_gradients[s->bar_gradient_index].top.b,
        200};

    for (i32 b = 0; b < s->num_bars; b++)
    {
        f64 p = s->peak_power[b];
        if (p <= 0)
        {
            continue;
        }

        f64 peak_db = 10.0 * log10(p + EPSILON_POWER) + DB_OFFSET;
        if (peak_db < DB_BOTTOM)
        {
            continue;
        }
        if (peak_db > DB_TOP)
        {
            peak_db = DB_TOP;
        }

        f64 norm = (f64)(peak_db - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        i32 y = h - (i32)(norm * h);
        i32 x = b * stride;
        DrawRectangle(x, y, BAR_PIXEL_WIDTH, 1, peak_color);
    }

    EndTextureMode();
}