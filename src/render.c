#include <math.h>
#include "render.h"

internal i32
freq_to_bar_index(const spectrum_state_t *s, f64 f)
{
    if (f < s->f_min)
    {
        f = s->f_min;
    }
    if (f > s->f_max)
    {
        f = s->f_max;
    }

    f64 r = log(f / s->f_min) / log(s->f_max / s->f_min);
    f64 pos = r * (f64)(s->num_bars - 1);
    i32 index = (i32)floor(pos + 0.5);
    if (index < 0)
    {
        index = 0;
    }
    if (index >= s->num_bars)
    {
        index = s->num_bars - 1;
    }

    return index;
}

internal void
draw_db_grid(const spectrum_state_t *s)
{
    for (f64 db = DB_BOTTOM; db <= DB_TOP; db += 12.0)
    {
        f64 norm = (db - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        i32 y = s->plot_top + (i32)(s->plot_height - norm * s->plot_height);
        DrawLine(s->plot_left, y, s->plot_left + s->plot_width, y, GRID_COLOR);

        char label[16];
        snprintf(label, sizeof(label), "%.0f", db);
        Vector2 ts = MeasureTextEx(s->font, label, 16, 0);
        i32 ly = y - (i32)(ts.y / 2);
        if (ly < 2)
        {
            ly = 2;
        }

        DrawTextEx(s->font, label, (Vector2){(f32)(s->plot_left - (i32)ts.x - 16), (f32)ly}, 20, 0, WHITE);
    }
}

internal void
draw_freq_grid(const spectrum_state_t *s)
{
    const f64 ticks[] = {20, 31, 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    i32 last_x = -999999;
    for (i32 i = 0; i < (i32)(sizeof(ticks) / sizeof(ticks[0])); i++)
    {
        f64 f = ticks[i];
        if (f < s->f_min || f > s->f_max)
        {
            continue;
        }

        i32 index = freq_to_bar_index(s, f);

        i32 x = s->plot_left + index * (BAR_PIXEL_WIDTH + BAR_GAP) + BAR_PIXEL_WIDTH / 2;
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
        i32 lx = x - (i32)(ts.x / 2);
        if (lx < s->plot_left)
        {
            lx = s->plot_left;
        }
        if (lx + (i32)ts.x > s->plot_left + s->plot_width)
        {
            lx = s->plot_left + s->plot_width - (i32)ts.x;
        }

        DrawTextEx(s->font, label, (Vector2){(f32)lx, (f32)(s->plot_top + s->plot_height + 6)}, 20, 0, WHITE);
    }
}
V

    internal void
    draw_overlay(const spectrum_state_t *s)
{
    char info[128];
    i32 sr = s->sample_rate;
    i32 denom = (i32)(1.0 / s->fractional_octave);
    if (denom <= 0)
    {
        denom = 1;
    }

    snprintf(info, sizeof(info), "Sample Rate: %d Hz | Fractional Oct. 1/%d", sr, denom);
    DrawTextEx(s->font, info, (Vector2){80, 16}, 25, 0, WHITE);

    char modes[128];
    char hold_buf[16];
    if (s->peak_hold_seconds <= 0.0)
    {
        snprintf(hold_buf, sizeof(hold_buf), "Off");
    }
    else
    {
        snprintf(hold_buf, sizeof(hold_buf), "%.1fs", s->peak_hold_seconds);
    }

    // Show averaging mode with attack/release in ms
    const char *avg_label = s->db_smoothing_enabled ? "dB" : "Lin";
    f64 attack_ms = s->db_smoothing_enabled ? s->db_smooth_attack_ms : s->smooth_attack_ms;
    f64 release_ms = s->db_smoothing_enabled ? s->db_smooth_release_ms : s->smooth_release_ms;

    snprintf(modes, sizeof(modes),
             "Avg: %s (%.0f/%.0f ms) | Pink: %s | Hold: %s",
             avg_label, attack_ms, release_ms,
             s->pinking_enabled ? "On" : "Off",
             hold_buf);

    DrawTextEx(s->font, modes, (Vector2){80, 44}, 20, 0, (Color){200, 200, 200, 255});

    char meters[96];
    const char *peak_txt;
    const char *rms_txt;
    char pkbuf[32], rmsbuf[32];

    if (isnan(s->meter_peak_dbfs))
    {
        peak_txt = "--.-";
    }
    else if (isinf(s->meter_peak_dbfs))
    {
        snprintf(pkbuf, sizeof(pkbuf), "-inf");
        peak_txt = pkbuf;
    }
    else
    {
        snprintf(pkbuf, sizeof(pkbuf), "%.1f", s->meter_peak_dbfs);
        peak_txt = pkbuf;
    }

    if (isnan(s->meter_rms_dbfs))
    {
        rms_txt = "--.-";
    }
    else if (isinf(s->meter_rms_dbfs))
    {
        snprintf(rmsbuf, sizeof(rmsbuf), "-inf");
        rms_txt = rmsbuf;
    }
    else
    {
        snprintf(rmsbuf, sizeof(rmsbuf), "%.1f", s->meter_rms_dbfs);
        rms_txt = rmsbuf;
    }

    snprintf(meters, sizeof(meters), "Peak: %6s dBFS\nRMS:  %6s dBFS", peak_txt, rms_txt);

    Vector2 ts = MeasureTextEx(s->font, meters, 20, 0);
    f32 x = (f32)(s->plot_left + s->plot_width - (i32)ts.x - 120);
    DrawTextEx(s->font, meters, (Vector2){x, 16}, 32, 0, WHITE);
}

void render_draw(const spectrum_state_t *s)
{
    draw_db_grid(s);
    draw_freq_grid(s);

    DrawTexturePro(s->fft_rt.texture,
                   (Rectangle){0, 0, (f32)s->fft_rt.texture.width, (f32)-s->fft_rt.texture.height},
                   (Rectangle){(f32)s->plot_left, (f32)s->plot_top, (f32)s->plot_width, (f32)s->plot_height},
                   (Vector2){0, 0}, 0, WHITE);

    draw_overlay(s);
}
