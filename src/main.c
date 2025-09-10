#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#include <raylib.h>
#include <fftw3.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FFT_WINDOW_SIZE 1024
#define BAR_PIXEL_WIDTH 1

#define FRACTIONAL_OCTAVE (1.0 / 48.0)
#define DB_TOP 0.0
#define DB_BOTTOM -96.0
#define PEAK_DECAY_DB_PER_SEC 3.0
#define LERP_SPEED 0.20
#define EPSILON_POWER 1e-12

#define BAR_GRADIENT_BOTTOM (Color){255, 100, 0, 255}
#define BAR_GRADIENT_TOP (Color){255, 255, 0, 255}

#define MARGIN_LEFT 60
#define MARGIN_RIGHT 20
#define MARGIN_TOP 28
#define MARGIN_BOTTOM 48

typedef struct spectrum_state
{
    // FFT
    double fft_in[FFT_WINDOW_SIZE];
    fftw_complex fft_out[FFT_WINDOW_SIZE / 2 + 1];
    fftw_plan fft_plan;
    int fft_bins;
    double bin_mag[FFT_WINDOW_SIZE / 2 + 1];

    // Bars
    int num_bars;
    double *bar_target;
    double *bar_smoothed;
    double *peak_power;
    double *bar_freq_center;

    // Frequency mapping
    double f_min;
    double f_max;
    double log_f_ratio;
    double fractional_k;

    // Timing
    double seconds_per_window;

    // UI
    Texture2D gradient_tex;
    RenderTexture2D fft_rt;
    int last_width;
    int last_height;
    int plot_left;
    int plot_top;
    int plot_width;
    int plot_height;
    Font font;
} spectrum_state;

// ---------- Utility --------------------------------------------------

static inline int
calc_num_bars_for_width(int w)
{
    if (w <= 0)
    {
        return 0;
    }

    return w / BAR_PIXEL_WIDTH;
}

static Texture2D
create_gradient_texture(int h)
{
    Image img = GenImageGradientLinear(1, h, 0, BAR_GRADIENT_TOP, BAR_GRADIENT_BOTTOM);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);

    return tex;
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

// ---------- Allocation / Init / Destroy ----------------------------------

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

static void
init_spectrum_state(spectrum_state *s, Wave *wave, Font font)
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

static void
destroy_spectrum_state(spectrum_state *s)
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

// ---------- FFT / Analysis ----------------------------------

static void
compute_fft_window(spectrum_state *s, float *samples, Wave *wave, int window_index)
{
    int start_index = window_index * FFT_WINDOW_SIZE * wave->channels;
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
        s->bar_target[b] = count ? (power_sum / count) : 0.0;
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

// ---------- Resize Handling --------------------------------------

static int
handle_resize(spectrum_state *s)
{
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    if (sw == s->last_width && sh == s->last_height)
    {
        return 1;
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

    return reallocate_bars_if_needed(s);
}

static int
freq_to_bar_index(const spectrum_state *s, double f)
{
    if (f < s->f_min)
    {
        f = s->f_min;
    }
    if (f > s->f_max)
    {
        f = s->f_max;
    }

    double x_norm = log(f / s->f_min) / s->log_f_ratio;
    double bar_pos = x_norm * (double)(s->num_bars - 1);
    int bar_index = (int)floor(bar_pos + 0.5);
    if (bar_index < 0)
    {
        bar_index = 0;
    }
    if (bar_index >= s->num_bars)
    {
        bar_index = s->num_bars - 1;
    }

    return bar_index;
}

static void
render_fft_to_texture(spectrum_state *s)
{
    BeginTextureMode(s->fft_rt);
    ClearBackground((Color){0, 0, 0, 0});
    int h = s->plot_height;
    for (int b = 0; b < s->num_bars; b++)
    {
        double power = s->bar_smoothed[b];
        double mag_db = 10.0 * log10(power + EPSILON_POWER);
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

    Color peak_color = (Color){BAR_GRADIENT_TOP.r, BAR_GRADIENT_TOP.g, BAR_GRADIENT_TOP.b, 160};
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

// ---------- Drawing ------------------------------------------

static void
draw_db_grid(const spectrum_state *s)
{
    Color grid_color = (Color){40, 40, 40, 96};
    for (double dB = DB_BOTTOM; dB <= DB_TOP; dB += 12.0)
    {
        double norm = (dB - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        int y = s->plot_top + (int)(s->plot_height - norm * s->plot_height);
        DrawLine(s->plot_left, y, s->plot_left + s->plot_width, y, grid_color);

        char label[16];
        snprintf(label, sizeof(label), "%.0f", dB);
        Vector2 ts = MeasureTextEx(s->font, label, 16, 0);
        int ly = y - (int)(ts.y / 2);
        if (ly < 2)
        {
            ly = 2;
        }
        DrawTextEx(s->font, label, (Vector2){(float)(s->plot_left - (int)ts.x - 6), (float)ly}, 16, 0, (Color){255, 255, 255, 255});
    }
}

static void
draw_freq_grid(const spectrum_state *s)
{
    const double freqs[] = {20, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};
    int nf = (int)(sizeof(freqs) / sizeof(freqs[0]));
    Color grid_color = (Color){40, 40, 40, 96};
    int last_x = -9999;
    for (int i = 0; i < nf; i++)
    {
        double f = freqs[i];
        if (f < s->f_min || f > s->f_max)
        {
            continue;
        }

        int idx = freq_to_bar_index(s, f);
        int x = s->plot_left + idx * BAR_PIXEL_WIDTH;
        if (x == last_x)
        {
            continue;
        }

        last_x = x;
        DrawLine(x, s->plot_top, x, s->plot_top + s->plot_height, grid_color);
        if (i % 2 == 0)
        {
            char label[16];
            if (f >= 1000.0)
            {
                snprintf(label, sizeof(label), "%.0fk", f / 1000.0);
            }
            else
            {
                snprintf(label, sizeof(label), "%.0f", f);
            }

            Vector2 ts = MeasureTextEx(s->font, label, 16, 0);
            int lx = x - (int)(ts.x / 2);
            if (lx < s->plot_left)
            {
                lx = s->plot_left;
            }
            if (lx + (int)ts.x > s->plot_left + s->plot_width)
            {
                lx = s->plot_left + s->plot_width - (int)ts.x;
            }

            DrawTextEx(s->font, label, (Vector2){(float)lx, (float)(s->plot_top + s->plot_height + 6)}, 16, 0, (Color){255, 255, 255, 255});
        }
    }
}

static void
draw_overlay_text(const spectrum_state *s)
{
    DrawTextEx(s->font, "1/48 Oct smoothing.", (Vector2){80, 4}, 20, 0, WHITE);
}

static const char *
read_input_file_path(int argc, char **argv)
{
    if (argc >= 2)
    {
        return argv[1];
    }

    return NULL;
}

int main(int argc, char **argv)
{
    printf("FFT Visualizer Starting...\n");

    const char *input_file = read_input_file_path(argc, argv);
    if (!input_file)
    {
        fprintf(stderr, "Usage: %s <wav file>\n", argv[0]);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);

    Font main_font = LoadFontEx("assets/fonts/retro-pixel-arcade.ttf", 64, 0, 250);

    Wave wave = LoadWave(input_file);
    if (wave.frameCount == 0)
    {
        fprintf(stderr, "Failed to load WAV: %s\n", input_file);
        CloseAudioDevice();
        CloseWindow();
        return 1;
    }

    Sound sound = LoadSound(input_file);
    PlaySound(sound);

    float *samples = LoadWaveSamples(wave);

    spectrum_state state;
    init_spectrum_state(&state, &wave, main_font);

    int total_windows = wave.frameCount / FFT_WINDOW_SIZE;
    int window_index = 0;
    double accumulator = 0.0;

    while (!WindowShouldClose() && window_index < total_windows)
    {
        double dt = GetFrameTime();

        if (!handle_resize(&state))
        {
            break;
        }

        accumulator += dt;
        if (accumulator >= state.seconds_per_window)
        {
            accumulator -= state.seconds_per_window;
            compute_fft_window(&state, samples, &wave, window_index);
            compute_bar_targets(&state, wave.sampleRate);
            window_index++;
        }

        smooth_bars(&state);
        update_peaks(&state, dt);
        render_fft_to_texture(&state);

        BeginDrawing();
        ClearBackground((Color){18, 18, 18, 255});
        DrawTexturePro(state.fft_rt.texture,
                       (Rectangle){0, 0, (float)state.fft_rt.texture.width, (float)-state.fft_rt.texture.height},
                       (Rectangle){(float)state.plot_left, (float)state.plot_top, (float)state.plot_width, (float)state.plot_height},
                       (Vector2){0, 0}, 0, WHITE);
        draw_db_grid(&state);
        draw_freq_grid(&state);
        draw_overlay_text(&state);
        EndDrawing();
    }

    UnloadWaveSamples(samples);
    UnloadWave(wave);
    StopSound(sound);
    UnloadSound(sound);
    destroy_spectrum_state(&state);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
