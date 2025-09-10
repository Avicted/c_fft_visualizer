#include <math.h>
#include "render.h"

static int
freq_to_bar_index(const spectrum_state_t *s, double f)
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
draw_db_grid(const spectrum_state_t *s)
{
    for (double dB = DB_BOTTOM; dB <= DB_TOP; dB += 12.0)
    {
        double norm = (dB - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        int y = s->plot_top + (int)(s->plot_height - norm * s->plot_height);
        DrawLine(s->plot_left, y, s->plot_left + s->plot_width, y, GRID_COLOR);

        char label[16];
        snprintf(label, sizeof(label), "%.0f", dB);
        Vector2 ts = MeasureTextEx(s->font, label, 16, 0);
        int ly = y - (int)(ts.y / 2);
        if (ly < 2)
        {
            ly = 2;
        }
        DrawTextEx(s->font, label, (Vector2){(float)(s->plot_left - (int)ts.x - 6), (float)ly}, 16, 0, WHITE);
    }
}

static void
draw_freq_grid(const spectrum_state_t *s)
{
    static const double freqs[] = {20, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000};
    int nf = (int)(sizeof(freqs) / sizeof(freqs[0]));
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
        DrawLine(x, s->plot_top, x, s->plot_top + s->plot_height, GRID_COLOR);
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

            DrawTextEx(s->font, label, (Vector2){(float)lx, (float)(s->plot_top + s->plot_height + 6)}, 16, 0, WHITE);
        }
    }
}

static void
draw_overlay(const spectrum_state_t *s)
{
    char *title = "Oct smoothing";
    char oct_smoothing_str[32];
    int denominator = (int)(1.0 / FRACTIONAL_OCTAVE + 0.5); // Calculate denominator dynamically
    snprintf(oct_smoothing_str, sizeof(oct_smoothing_str), "1/%d", denominator);
    char final_str[64];
    snprintf(final_str, sizeof(final_str), "%s: %s", title, oct_smoothing_str);

    DrawTextEx(s->font, final_str, (Vector2){80, 16}, 20, 0, WHITE);
}

void render_draw(const spectrum_state_t *s)
{
    DrawTexturePro(s->fft_rt.texture,
                   (Rectangle){0, 0, (float)s->fft_rt.texture.width, (float)-s->fft_rt.texture.height},
                   (Rectangle){(float)s->plot_left, (float)s->plot_top, (float)s->plot_width, (float)s->plot_height},
                   (Vector2){0, 0}, 0, WHITE);
    draw_db_grid(s);
    draw_freq_grid(s);
    draw_overlay(s);
}
