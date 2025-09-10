#include <raylib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <fftw3.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FFT_WINDOW_SIZE 1024
#define BAR_PIXEL_WIDTH 1

#define FRACTIONAL_OCTAVE (1.0 / 48.0)
#define DB_TOP 0.0
#define DB_BOTTOM -96.0
#define PEAK_DECAY_DB_PER_SEC 3.0

// #define BAR_GRADIENT_BOTTOM (Color){0, 64, 255, 255}
// #define BAR_GRADIENT_TOP (Color){0, 230, 255, 255}
#define BAR_GRADIENT_BOTTOM (Color){255, 100, 0, 255}
#define BAR_GRADIENT_TOP (Color){255, 255, 0, 255}

static inline int
calc_num_bars(void)
{
    return GetScreenWidth() / BAR_PIXEL_WIDTH;
}

int main(void)
{
    printf("FFT Visualizer Starting...\n");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);

    Font main_font = LoadFontEx("assets/fonts/retro-pixel-arcade.ttf", 64, 0, 250);

    const char *input_file_path = "music.wav";
    Wave wave = LoadWave(input_file_path);
    if (wave.frameCount == 0)
    {
        fprintf(stderr, "Failed to load WAV: %s\n", input_file_path);
        return 1;
    }

    Sound sound = LoadSound(input_file_path);
    PlaySound(sound);

    float *samples = LoadWaveSamples(wave);
    float mono_buffer[FFT_WINDOW_SIZE];

    int total_windows = wave.frameCount / FFT_WINDOW_SIZE;

    double in[FFT_WINDOW_SIZE];
    fftw_complex out[FFT_WINDOW_SIZE / 2 + 1];
    fftw_plan plan = fftw_plan_dft_r2c_1d(FFT_WINDOW_SIZE, in, out, FFTW_ESTIMATE);

    int fft_bins = FFT_WINDOW_SIZE / 2 + 1;
    double bin_mag[FFT_WINDOW_SIZE / 2 + 1] = {0};

    int num_bars = calc_num_bars();
    double *bar_target = (double *)calloc(num_bars, sizeof(double));
    double *bar_smoothed = (double *)calloc(num_bars, sizeof(double));
    double *peak_power = (double *)calloc(num_bars, sizeof(double));

    double lerp_speed = 0.20;

    float seconds_per_window = (float)FFT_WINDOW_SIZE / wave.sampleRate;
    int w = 0;
    float accumulator = 0.0f;

    const double f_min = 20.0;
    const double f_max = wave.sampleRate / 2.0;
    const double log_f_ratio = log(f_max / f_min);

    int last_width = GetScreenWidth();
    int last_height = GetScreenHeight();

    Texture2D gradient_tex = {0};
    {
        Image img = GenImageGradientLinear(1, last_height, 0, BAR_GRADIENT_TOP, BAR_GRADIENT_BOTTOM);
        gradient_tex = LoadTextureFromImage(img);
        UnloadImage(img);
    }

    while (!WindowShouldClose() && w < total_windows)
    {
        int cur_width = GetScreenWidth();
        int cur_height = GetScreenHeight();
        if (cur_width != last_width || cur_height != last_height)
        {
            if (cur_width != last_width)
            {
                int new_num = calc_num_bars();
                if (new_num < 2)
                {
                    new_num = 2;
                }
                if (new_num != num_bars)
                {
                    double *new_target = (double *)calloc(new_num, sizeof(double));
                    if (!new_target)
                    {
                        fprintf(stderr, "Memory allocation failed for new_target.\n");
                        break;
                    }

                    double *new_smooth = (double *)calloc(new_num, sizeof(double));
                    if (!new_smooth)
                    {
                        fprintf(stderr, "Memory allocation failed for new_smooth.\n");
                        free(new_target);
                        break;
                    }

                    double *new_peak = (double *)calloc(new_num, sizeof(double));
                    if (!new_peak)
                    {
                        fprintf(stderr, "Memory allocation failed for new_peak.\n");
                        free(new_target);
                        free(new_smooth);
                        break;
                    }

                    for (int i = 0; i < new_num; i++)
                    {
                        double t = (double)i / (double)(new_num - 1);
                        int old_index = (int)round(t * (num_bars - 1));
                        if (old_index < 0)
                        {
                            old_index = 0;
                        }
                        if (old_index >= num_bars)
                        {
                            old_index = num_bars - 1;
                        }

                        new_target[i] = bar_target[old_index];
                        new_smooth[i] = bar_smoothed[old_index];
                        new_peak[i] = peak_power[old_index];
                    }

                    free(bar_target);
                    free(bar_smoothed);
                    free(peak_power);
                    bar_target = new_target;
                    bar_smoothed = new_smooth;
                    peak_power = new_peak;
                    num_bars = new_num;
                }

                last_width = cur_width;
            }
            // Rebuild gradient if height changed
            if (cur_height != last_height)
            {
                if (gradient_tex.id)
                {
                    UnloadTexture(gradient_tex);
                }

                Image img = GenImageGradientLinear(1, cur_height, 0, BAR_GRADIENT_TOP, BAR_GRADIENT_BOTTOM);
                gradient_tex = LoadTextureFromImage(img);
                UnloadImage(img);
                last_height = cur_height;
            }
        }

        float dt = GetFrameTime();
        accumulator += dt;

        if (accumulator >= seconds_per_window)
        {
            accumulator -= seconds_per_window;

            int start_index = w * FFT_WINDOW_SIZE * wave.channels;
            for (int i = 0; i < FFT_WINDOW_SIZE; i++)
            {
                if (wave.channels == 1)
                {
                    mono_buffer[i] = samples[start_index + i];
                }
                else
                {
                    mono_buffer[i] = 0.5f * (samples[start_index + (i * 2)] +
                                             samples[start_index + (i * 2) + 1]);
                }

                in[i] = mono_buffer[i];
            }

            fftw_execute(plan);

            for (int i = 0; i < fft_bins; i++)
            {
                double re = out[i][0];
                double im = out[i][1];
                bin_mag[i] = sqrt(re * re + im * im) / FFT_WINDOW_SIZE;
            }

            for (int b = 0; b < num_bars; b++)
            {
                double t = (double)b / (double)(num_bars - 1);
                double f_center = f_min * exp(log_f_ratio * t);

                double k = pow(2.0, FRACTIONAL_OCTAVE / 2.0);
                double f_low = f_center / k;
                double f_high = f_center * k;
                if (f_low < f_min)
                {
                    f_low = f_min;
                }
                if (f_high > f_max)
                {
                    f_high = f_max;
                }

                int bin_low = (int)ceil(f_low * FFT_WINDOW_SIZE / (double)wave.sampleRate);
                int bin_high = (int)floor(f_high * FFT_WINDOW_SIZE / (double)wave.sampleRate);
                if (bin_low < 1)
                {
                    bin_low = 1;
                }
                if (bin_high >= fft_bins)
                {
                    bin_high = fft_bins - 1;
                }
                if (bin_high < bin_low)
                {
                    bin_high = bin_low;
                }

                double power_sum = 0.0;
                int count = 0;
                for (int kbin = bin_low; kbin <= bin_high; kbin++)
                {
                    double a = bin_mag[kbin];
                    power_sum += a * a;
                    count++;
                }

                bar_target[b] = (count > 0) ? (power_sum / count) : 0.0;
            }

            w++;
        }

        for (int b = 0; b < num_bars; b++)
        {
            bar_smoothed[b] += lerp_speed * (bar_target[b] - bar_smoothed[b]);
        }

        {
            float dt = GetFrameTime();
            double decay_factor = pow(10.0, -PEAK_DECAY_DB_PER_SEC * dt / 10.0);
            for (int b = 0; b < num_bars; b++)
            {
                if (bar_smoothed[b] > peak_power[b])
                {
                    peak_power[b] = bar_smoothed[b];
                }
                else
                {
                    peak_power[b] *= decay_factor;
                    if (peak_power[b] < bar_smoothed[b])
                    {
                        peak_power[b] = bar_smoothed[b];
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){18, 18, 18, 255});

        int screen_height = GetScreenHeight();

        for (int b = 0; b < num_bars; b++)
        {
            double power = bar_smoothed[b];
            double mag_dB = 10.0 * log10(power + 1e-12);
            if (mag_dB < DB_BOTTOM)
            {
                mag_dB = DB_BOTTOM;
            }
            if (mag_dB > DB_TOP)
            {
                mag_dB = DB_TOP;
            }

            double norm = (mag_dB - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
            int bar_h = (int)(norm * screen_height);
            if (bar_h <= 0)
            {
                continue;
            }

            int x = b * BAR_PIXEL_WIDTH;
            Rectangle src = {0, (float)(gradient_tex.height - bar_h), 1, (float)bar_h};
            Rectangle dst = {(float)x, (float)(screen_height - bar_h), (float)BAR_PIXEL_WIDTH, (float)bar_h};
            DrawTexturePro(gradient_tex, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
        }

        for (int b = 0; b < num_bars; b++)
        {
            double p = peak_power[b];
            if (p <= 0)
            {
                continue;
            }

            double peak_dB = 10.0 * log10(p + 1e-12);
            if (peak_dB < DB_BOTTOM)
            {
                continue;
            }
            if (peak_dB > DB_TOP)
            {
                peak_dB = DB_TOP;
            }

            double norm = (peak_dB - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
            int y = screen_height - (int)(norm * screen_height);
            int x = b * BAR_PIXEL_WIDTH;
            Color peak_color = (Color){BAR_GRADIENT_TOP.r, BAR_GRADIENT_TOP.g, BAR_GRADIENT_TOP.b, 128};
            DrawRectangle(x, y, BAR_PIXEL_WIDTH, 1, peak_color);
        }

        // Horizontal dB lines
        Color grid_color = (Color){40, 40, 40, 64};
        for (double dB = DB_BOTTOM; dB <= DB_TOP; dB += 12.0)
        {
            double norm = (dB - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
            int y = screen_height - (int)(norm * screen_height);
            DrawLine(0, y, GetScreenWidth(), y, grid_color);
            char label[16];
            snprintf(label, sizeof(label), "%.0f", dB);
            Vector2 text_size = MeasureTextEx(main_font, label, 16, 0);
            DrawRectangle(0, y - 10, text_size.x + 8, 18, (Color){18, 18, 18, 220});
            DrawTextEx(main_font, label, (Vector2){4, y - 8}, 16, 0, (Color){0, 0, 0, 255});
            DrawTextEx(main_font, label, (Vector2){5, y - 9}, 16, 0, (Color){255, 255, 255, 255});
        }

        // Vertical frequency lines + labels
        {
            const double freqs[] = {20, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};
            int nf = (int)(sizeof(freqs) / sizeof(freqs[0]));
            int last_x = -10000;

            for (int i = 0; i < nf; i++)
            {
                double f = freqs[i];
                if (f < f_min || f > f_max)
                {
                    continue;
                }

                // Map frequency -> fractional bar index using SAME transform as bar center mapping (t = b/(numBars-1))
                double x_norm = log(f / f_min) / log_f_ratio;     // 0..1
                double bar_pos = x_norm * (double)(num_bars - 1); // float bar index
                int bar_index = (int)floor(bar_pos + 0.5);        // nearest bar center
                if (bar_index < 0)
                {
                    bar_index = 0;
                }
                if (bar_index >= num_bars)
                {
                    bar_index = num_bars - 1;
                }

                // Pixel x of the bar's LEFT edge (keeps line exactly on bar column)
                int x = bar_index * BAR_PIXEL_WIDTH;

                // Avoid duplicate lines if multiple freqs fall in same bar
                if (x == last_x)
                {
                    continue;
                }
                last_x = x;

                DrawLine(x, 0, x, screen_height, grid_color);

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

                    Vector2 ts = MeasureTextEx(main_font, label, 16, 0);
                    int lx = x + (BAR_PIXEL_WIDTH / 2) - (int)(ts.x / 2);
                    if (lx < 0)
                    {
                        lx = 0;
                    }
                    if (lx + (int)ts.x + 4 > GetScreenWidth())
                    {
                        lx = GetScreenWidth() - (int)ts.x - 4;
                    }

                    DrawRectangle(lx - 2, screen_height - 20, (int)ts.x + 4, 18, (Color){18, 18, 18, 220});
                    DrawTextEx(main_font, label, (Vector2){lx, screen_height - 18}, 16, 0, (Color){0, 0, 0, 255});
                    DrawTextEx(main_font, label, (Vector2){lx, screen_height - 19}, 16, 0, (Color){255, 255, 255, 255});
                }
            }
        }

        DrawTextEx(main_font, "FFT Visualizer (Fractional-Octave RTA)", (Vector2){10, 10}, 24, 0, WHITE);
        DrawTextEx(main_font, "Resize: bars adapt. 1/48 Oct smoothing.", (Vector2){10, 34}, 18, 0, (Color){200, 200, 200, 255});
        EndDrawing();
    }

    if (gradient_tex.id)
    {
        UnloadTexture(gradient_tex);
    }
    free(bar_target);
    free(bar_smoothed);
    free(peak_power);
    fftw_destroy_plan(plan);
    UnloadWaveSamples(samples);
    UnloadWave(wave);
    StopSound(sound);
    UnloadSound(sound);
    CloseAudioDevice();
    CloseWindow();

    return 0;
}
