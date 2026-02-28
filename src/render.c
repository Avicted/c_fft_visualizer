#include <math.h>
#include "render.h"

internal f64
power_to_db(f64 power)
{
    return 10.0 * log10(power + EPSILON_POWER) + DB_OFFSET;
}

internal f32
ui_text(f32 base)
{
    return base * (f32)UI_SCALE;
}

internal i32
ui_px(i32 base)
{
    return (i32)lround((f64)base * UI_SCALE);
}

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
    const f32 grid_label_size = ui_text(20.0f);
    const i32 label_left_pad = ui_px(16);

    for (f64 db = DB_BOTTOM; db <= DB_TOP; db += 12.0)
    {
        f64 norm = (db - DB_BOTTOM) / (DB_TOP - DB_BOTTOM);
        i32 y = s->plot_top + (i32)(s->plot_height - norm * s->plot_height);
        DrawLine(s->plot_left, y, s->plot_left + s->plot_width, y, GRID_COLOR);

        char label[16];
        snprintf(label, sizeof(label), "%.0f", db);
        Vector2 ts = MeasureTextEx(s->font, label, grid_label_size, 0);
        i32 ly = y - (i32)(ts.y / 2);
        if (ly < 2)
        {
            ly = 2;
        }

        DrawTextEx(s->font, label, (Vector2){(f32)(s->plot_left - (i32)ts.x - label_left_pad), (f32)ly}, grid_label_size, 0, WHITE);
    }
}

internal void
draw_freq_grid(const spectrum_state_t *s)
{
    const f32 grid_label_size = ui_text(20.0f);
    const i32 label_top_pad = ui_px(6);

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

        Vector2 ts = MeasureTextEx(s->font, label, grid_label_size, 0);
        i32 lx = x - (i32)(ts.x / 2);
        if (lx < s->plot_left)
        {
            lx = s->plot_left;
        }
        if (lx + (i32)ts.x > s->plot_left + s->plot_width)
        {
            lx = s->plot_left + s->plot_width - (i32)ts.x;
        }

        DrawTextEx(s->font, label, (Vector2){(f32)lx, (f32)(s->plot_top + s->plot_height + label_top_pad)}, grid_label_size, 0, WHITE);
    }
}

internal void
draw_overlay(const spectrum_state_t *s, i32 cursor_lock_enabled, i32 cursor_locked_index, i32 cursor_hover_index)
{
    const f32 info_text_size = ui_text(20.0f);
    const f32 mode_text_size = ui_text(18.0f);
    const f32 meter_text_size = ui_text(20.0f);
    const f32 cursor_text_size = ui_text(19.0f);

    char info[192];
    i32 sr = s->sample_rate;
    i32 denom = (i32)(1.0 / s->fractional_octave);
    if (denom <= 0)
    {
        denom = 1;
    }

    snprintf(info, sizeof(info), "Sample Rate: %d Hz | Fractional Oct. 1/%d", sr, denom);

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

    const char *freq_w = "Z";
    if (s->frequency_weighting_mode == FREQ_WEIGHTING_A)
    {
        freq_w = "A";
    }
    else if (s->frequency_weighting_mode == FREQ_WEIGHTING_C)
    {
        freq_w = "C";
    }

    const char *time_w = "Fast";
    if (s->time_weighting_mode == TIME_WEIGHTING_SLOW)
    {
        time_w = "Slow";
    }
    else if (s->time_weighting_mode == TIME_WEIGHTING_IMPULSE)
    {
        time_w = "Impulse";
    }

    snprintf(modes, sizeof(modes),
             "Avg: %s (%.0f/%.0f ms) | Pink: %s | Hold: %s | W: %s | T: %s | Cal: %s",
             avg_label, attack_ms, release_ms,
             s->pinking_enabled ? "On" : "Off",
             hold_buf,
             freq_w,
             time_w,
             s->spl_calibrated ? "On" : "Off");

    Vector2 info_size = MeasureTextEx(s->font, info, info_text_size, 0);
    Vector2 mode_size = MeasureTextEx(s->font, modes, mode_text_size, 0);
    i32 panel_left = ui_px(72);
    i32 panel_top = ui_px(12);
    i32 panel_w = (i32)fmax(info_size.x, mode_size.x) + ui_px(24);
    i32 panel_h = ui_px(58);
    DrawRectangle(panel_left, panel_top, panel_w, panel_h, (Color){0, 0, 0, 155});
    DrawRectangleLines(panel_left, panel_top, panel_w, panel_h, (Color){80, 80, 80, 200});
    DrawTextEx(s->font, info, (Vector2){(f32)(panel_left + ui_px(12)), (f32)(panel_top + ui_px(8))}, info_text_size, 0, WHITE);
    DrawTextEx(s->font, modes, (Vector2){(f32)(panel_left + ui_px(12)), (f32)(panel_top + ui_px(30))}, mode_text_size, 0, (Color){210, 210, 210, 255});

    char meters[192];
    const char *peak_txt;
    const char *rms_txt;
    const char *peak_spl_txt;
    const char *rms_spl_txt;
    char pkbuf[32], rmsbuf[32], pksplbuf[32], rmssplbuf[32];

    if (isnan(s->meter_peak_dbfs_display))
    {
        peak_txt = "--.-";
    }
    else if (isinf(s->meter_peak_dbfs_display))
    {
        snprintf(pkbuf, sizeof(pkbuf), "-inf");
        peak_txt = pkbuf;
    }
    else
    {
        snprintf(pkbuf, sizeof(pkbuf), "%.1f", s->meter_peak_dbfs_display);
        peak_txt = pkbuf;
    }

    if (isnan(s->meter_rms_dbfs_display))
    {
        rms_txt = "--.-";
    }
    else if (isinf(s->meter_rms_dbfs_display))
    {
        snprintf(rmsbuf, sizeof(rmsbuf), "-inf");
        rms_txt = rmsbuf;
    }
    else
    {
        snprintf(rmsbuf, sizeof(rmsbuf), "%.1f", s->meter_rms_dbfs_display);
        rms_txt = rmsbuf;
    }

    if (!s->spl_calibrated || isnan(s->meter_peak_dbspl_display))
    {
        peak_spl_txt = "--.-";
    }
    else if (isinf(s->meter_peak_dbspl_display))
    {
        snprintf(pksplbuf, sizeof(pksplbuf), "-inf");
        peak_spl_txt = pksplbuf;
    }
    else
    {
        snprintf(pksplbuf, sizeof(pksplbuf), "%.1f", s->meter_peak_dbspl_display);
        peak_spl_txt = pksplbuf;
    }

    if (!s->spl_calibrated || isnan(s->meter_rms_dbspl_display))
    {
        rms_spl_txt = "--.-";
    }
    else if (isinf(s->meter_rms_dbspl_display))
    {
        snprintf(rmssplbuf, sizeof(rmssplbuf), "-inf");
        rms_spl_txt = rmssplbuf;
    }
    else
    {
        snprintf(rmssplbuf, sizeof(rmssplbuf), "%.1f", s->meter_rms_dbspl_display);
        rms_spl_txt = rmssplbuf;
    }

    snprintf(meters, sizeof(meters),
             "Peak: %6s dBFS  %6s dBSPL\nRMS:  %6s dBFS  %6s dBSPL",
             peak_txt, peak_spl_txt,
             rms_txt, rms_spl_txt);

    Vector2 meter_size = MeasureTextEx(s->font, meters, meter_text_size, 0);
    i32 meter_panel_w = (i32)meter_size.x + ui_px(24);
    i32 meter_panel_h = (i32)meter_size.y + ui_px(14);
    i32 meter_panel_x = s->plot_left + s->plot_width - meter_panel_w - ui_px(12);
    i32 meter_panel_y = ui_px(12);
    DrawRectangle(meter_panel_x, meter_panel_y, meter_panel_w, meter_panel_h, (Color){0, 0, 0, 155});
    DrawRectangleLines(meter_panel_x, meter_panel_y, meter_panel_w, meter_panel_h, (Color){80, 80, 80, 200});
    DrawTextEx(s->font, meters, (Vector2){(f32)(meter_panel_x + ui_px(12)), (f32)(meter_panel_y + ui_px(8))}, meter_text_size, 0, WHITE);

    i32 active_index = -1;
    if (cursor_lock_enabled && cursor_locked_index >= 0 && cursor_locked_index < s->num_bars)
    {
        active_index = cursor_locked_index;
    }
    else if (cursor_hover_index >= 0 && cursor_hover_index < s->num_bars)
    {
        active_index = cursor_hover_index;
    }

    if (active_index >= 0)
    {
        local_persist i32 cursor_display_index = -1;
        local_persist f64 cursor_live_db_display = DB_BOTTOM;
        local_persist f64 cursor_max_db_display = DB_BOTTOM;

        f64 f = s->bar_freq_center[active_index];
        f64 live_db_target = power_to_db(s->bar_smoothed[active_index]);
        f64 max_db_target = power_to_db(s->max_hold_power[active_index]);
        if (live_db_target < DB_BOTTOM)
        {
            live_db_target = DB_BOTTOM;
        }
        if (max_db_target < DB_BOTTOM)
        {
            max_db_target = DB_BOTTOM;
        }

        if (cursor_display_index != active_index)
        {
            cursor_live_db_display = live_db_target;
            cursor_max_db_display = max_db_target;
            cursor_display_index = active_index;
        }
        else
        {
            f64 dt = GetFrameTime();
            f64 tau = CURSOR_READOUT_SMOOTH_MS * 0.001;
            f64 alpha = (tau > 0.0) ? (1.0 - exp(-dt / tau)) : 1.0;
            cursor_live_db_display += alpha * (live_db_target - cursor_live_db_display);
            cursor_max_db_display += alpha * (max_db_target - cursor_max_db_display);
        }

        f64 live_db = cursor_live_db_display;
        f64 max_db = cursor_max_db_display;
        if (live_db < DB_BOTTOM)
        {
            live_db = DB_BOTTOM;
        }
        if (max_db < DB_BOTTOM)
        {
            max_db = DB_BOTTOM;
        }

        char fbuf[32];
        if (f >= 1000.0)
        {
            snprintf(fbuf, sizeof(fbuf), "%.3fk", f / 1000.0);
        }
        else
        {
            snprintf(fbuf, sizeof(fbuf), "%.1f", f);
        }

        char cursor_info[192];
        const char *mode = cursor_lock_enabled ? "LOCK" : "HOVER";
        snprintf(cursor_info, sizeof(cursor_info),
                 "%s  %s Hz  |  Live %5.1f dB  |  Max %5.1f dB",
                 mode, fbuf, live_db, max_db);

        Vector2 cursor_size = MeasureTextEx(s->font, cursor_info, cursor_text_size, 0);
        i32 cursor_panel_x = ui_px(72);
        i32 cursor_panel_y = s->plot_top + s->plot_height - ui_px(42);
        i32 cursor_panel_w = (i32)cursor_size.x + ui_px(22);
        i32 cursor_panel_h = ui_px(32);
        DrawRectangle(cursor_panel_x, cursor_panel_y, cursor_panel_w, cursor_panel_h, (Color){0, 0, 0, 155});
        DrawRectangleLines(cursor_panel_x, cursor_panel_y, cursor_panel_w, cursor_panel_h, (Color){80, 80, 80, 200});
        DrawTextEx(s->font, cursor_info, (Vector2){(f32)(cursor_panel_x + ui_px(11)), (f32)(cursor_panel_y + ui_px(7))}, cursor_text_size, 0, WHITE);

        i32 stride = BAR_PIXEL_WIDTH + BAR_GAP;
        i32 cx = s->plot_left + active_index * stride + BAR_PIXEL_WIDTH / 2;
        Color cursor_line = cursor_lock_enabled ? (Color){255, 220, 80, 220} : (Color){255, 255, 255, 110};
        DrawLine(cx, s->plot_top, cx, s->plot_top + s->plot_height, cursor_line);
    }
}

void render_draw(const spectrum_state_t *s, i32 cursor_lock_enabled, i32 cursor_locked_index, i32 cursor_hover_index)
{
    draw_db_grid(s);
    draw_freq_grid(s);

    DrawTexturePro(s->fft_rt.texture,
                   (Rectangle){0, 0, (f32)s->fft_rt.texture.width, (f32)-s->fft_rt.texture.height},
                   (Rectangle){(f32)s->plot_left, (f32)s->plot_top, (f32)s->plot_width, (f32)s->plot_height},
                   (Vector2){0, 0}, 0, WHITE);

    draw_overlay(s, cursor_lock_enabled, cursor_locked_index, cursor_hover_index);
}
