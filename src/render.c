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

    double r = log(f / s->f_min) / log(s->f_max / s->f_min);
    double pos = r * (double)(s->num_bars - 1);
    int idx = (int)floor(pos + 0.5);
    if (idx < 0)
    {
        idx = 0;
    }
    if (idx >= s->num_bars)
    {
        idx = s->num_bars - 1;
    }

    return idx;
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

        DrawTextEx(s->font, label, (Vector2){(float)(s->plot_left - (int)ts.x - 6), (float)ly}, 20, 0, WHITE);
    }
}

static void
draw_freq_grid(const spectrum_state_t *s)
{
    const double ticks[] = {20, 31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    int last_x = -999999;
    for (int i = 0; i < (int)(sizeof(ticks) / sizeof(ticks[0])); i++)
    {
        double f = ticks[i];
        if (f < s->f_min || f > s->f_max)
        {
            continue;
        }

        int idx = freq_to_bar_index(s, f);

        int x = s->plot_left + idx * (BAR_PIXEL_WIDTH + BAR_GAP) + BAR_PIXEL_WIDTH / 2;
        if (x == last_x)
        {
            continue;
        }

        last_x = x;
        DrawLine(x, s->plot_top, x, s->plot_top + s->plot_height, GRID_COLOR);

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

        DrawTextEx(s->font, label, (Vector2){(float)lx, (float)(s->plot_top + s->plot_height + 6)}, 20, 0, WHITE);
    }
}

static void
draw_overlay(const spectrum_state_t *s)
{
    char info[96];
    int sr = s->sample_rate;
    int denom = (int)(1.0 / s->fractional_octave);
    if (denom <= 0)
    {
        denom = 1;
    }

    snprintf(info, sizeof(info),
             "Sample Rate: %d Hz | Fractional Oct. 1/%d",
             sr, denom);

    DrawTextEx(s->font, info, (Vector2){80, 16}, 25, 0, WHITE);
}

void render_draw(const spectrum_state_t *s)
{
    draw_db_grid(s);
    draw_freq_grid(s);

    DrawTexturePro(s->fft_rt.texture,
                   (Rectangle){0, 0, (float)s->fft_rt.texture.width, (float)-s->fft_rt.texture.height},
                   (Rectangle){(float)s->plot_left, (float)s->plot_top, (float)s->plot_width, (float)s->plot_height},
                   (Vector2){0, 0}, 0, WHITE);

    draw_overlay(s);
}
