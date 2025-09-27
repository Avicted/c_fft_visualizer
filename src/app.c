#include "app.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

internal void
print_usage(const char *prog)
{
    fprintf(stderr,
            "Usage:      %s <wav-file> [options]\n"
            "Live Usage: %s --mic\n"
            "Options:\n"
            "  -h, --help       Show this help and exit\n"
            "  -l, --loop       Loop playback\n"
            "\n"
            "Controls:\n"
            "  O   Octave (1/1…1/48)\n"
            "  C   Colors\n"
            "  P   Pink compensation\n"
            "  A   dB averaging\n"
            "  F   Fast/Slow preset\n"
            "  H   Peak-hold\n"
            "  F11 Fullscreen\n",
            prog, prog);
}

void app_parse_input_args(i32 argc, char **argv, char **input_file, i32 *loop_flag, i32 *mic_mode)
{
    *input_file = NULL;
    *loop_flag = 0;
    *mic_mode = 0;

    if (argc <= 1)
    {
        print_usage(argv[0]);
        exit(0);
    }

    for (i32 i = 1; i < argc; i++)
    {
        const char *arg = argv[i];

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0)
        {
            print_usage(argv[0]);
            exit(0);
        }
        else if (strcmp(arg, "--loop") == 0 || strcmp(arg, "-l") == 0)
        {
            *loop_flag = 1;
        }
        else if (strcmp(arg, "--mic") == 0 || strcmp(arg, "-m") == 0)
        {
            *mic_mode = 1;
        }
        else if (arg[0] != '-' && !*input_file)
        {
            *input_file = argv[i];
        }
        else
        {
            fprintf(stderr, "Unknown argument: %s\n\n", arg);
            print_usage(argv[0]);
            exit(1);
        }
    }

    if (!*input_file && !*mic_mode)
    {
        fprintf(stderr, "Error: missing input WAV file (or use --mic).\n\n");
        print_usage(argv[0]);
        exit(1);
    }
}

void app_handle_input(app_state_t *app_state)
{
    if (IsKeyPressed(KEY_F11))
    {
        if (IsWindowFullscreen())
        {
            ToggleFullscreen();
            SetWindowSize(app_state->windowed_w, app_state->windowed_h);
        }
        else
        {
            app_state->windowed_w = GetScreenWidth();
            app_state->windowed_h = GetScreenHeight();
            i32 monitor = GetCurrentMonitor();
            i32 mw = GetMonitorWidth(monitor);
            i32 mh = GetMonitorHeight(monitor);
            SetWindowSize(mw, mh);
            ToggleFullscreen();
        }

        spectrum_handle_resize(&app_state->spectrum_state);
    }

    if (IsKeyPressed(KEY_O))
    {
        app_state->fractional_octave_index_selected++;
        if (app_state->fractional_octave_index_selected >= NUM_FRACTIONAL_OCTAVES)
        {
            app_state->fractional_octave_index_selected = 0;
        }

        f64 frac = FRACTIONAL_OCTAVES[app_state->fractional_octave_index_selected];
        spectrum_set_fractional_octave(&app_state->spectrum_state,
                                       frac,
                                       app_state->fractional_octave_index_selected);

        spectrum_handle_resize(&app_state->spectrum_state);
    }

    if (IsKeyPressed(KEY_C))
    {
        spectrum_state_t *s = &app_state->spectrum_state;
        s->bar_gradient_index = (s->bar_gradient_index + 1) % NUM_BAR_GRADIENTS;
        if (s->gradient_tex.id)
        {
            UnloadTexture(s->gradient_tex);
        }
        s->gradient_tex = create_gradient_texture(s->plot_height, s->bar_gradients[s->bar_gradient_index]);
    }

    if (IsKeyPressed(KEY_P))
    {
        app_state->spectrum_state.pinking_enabled ^= 1;
    }

    if (IsKeyPressed(KEY_A))
    {
        app_state->spectrum_state.db_smoothing_enabled ^= 1;
    }

    if (IsKeyPressed(KEY_F))
    {
        spectrum_state_t *s = &app_state->spectrum_state;
        local_persist i32 fast = 0;
        fast ^= 1;
        if (fast)
        {
            s->db_smooth_attack_ms = 20.0;
            s->db_smooth_release_ms = 600.0;
        }
        else
        {
            s->db_smooth_attack_ms = 10.0;
            s->db_smooth_release_ms = 1200.0;
        }

        TraceLog(LOG_INFO, "Averaging preset: %s (attack %.0f ms, release %.0f ms)",
                 fast ? "Fast" : "Slow",
                 s->db_smooth_attack_ms, s->db_smooth_release_ms);
    }

    if (IsKeyPressed(KEY_H))
    {
        spectrum_state_t *s = &app_state->spectrum_state;
        f64 next = s->peak_hold_seconds;

        if (next <= 0.0)
        {
            next = 0.5;
        }
        else if (next < 1.0)
        {
            next = 1.0;
        }
        else if (next < 2.0)
        {
            next = 2.0;
        }
        else
        {
            next = 0.0;
        }

        spectrum_set_peak_hold_seconds(s, next);
    }
}

internal inline ul
mic_ring_count(const app_state_t *app)
{
    return app->mic_ring_write - app->mic_ring_read;
}

internal inline ul
mic_ring_space(const app_state_t *app)
{
    return app->mic_ring_capacity - mic_ring_count(app);
}

internal ul
mic_ring_push(app_state_t *app, const f32 *src, ul n)
{
    ul space = mic_ring_space(app);
    if (n > space)
    {
        n = space; // drop overflow
    }

    ul cap = app->mic_ring_capacity;
    ul w = app->mic_ring_write;
    for (ul i = 0; i < n; i++)
    {
        app->mic_ring[(w + i) % cap] = src[i];
    }

    app->mic_ring_write = w + n;
    return n;
}

internal ul
mic_ring_pop(app_state_t *app, f32 *dst, ul n)
{
    ul avail = mic_ring_count(app);
    if (n > avail)
    {
        n = avail;
    }

    ul cap = app->mic_ring_capacity;
    ul r = app->mic_ring_read;
    for (ul i = 0; i < n; i++)
    {
        dst[i] = app->mic_ring[(r + i) % cap];
    }

    app->mic_ring_read = r + n;
    return n;
}

internal i32
audio_callback(
    const void *input_buffer,
    void *output_buffer,
    unsigned long frames_per_buffer,
    const PaStreamCallbackTimeInfo *time_info,
    PaStreamCallbackFlags status_flags,
    void *user_data)
{
    (void)output_buffer;
    (void)time_info;
    (void)status_flags;

    app_state_t *app = (app_state_t *)user_data;

    // If no input, nothing to push
    if (!input_buffer || !app || !app->mic_ring)
    {
        return (int)paContinue;
    }

    const f32 *in = (const f32 *)input_buffer;

    // Downmix to mono if needed (config uses INPUT_NUM_CHANNELS = 1)
#if INPUT_NUM_CHANNELS == 1
    (void)frames_per_buffer;
    mic_ring_push(app, in, (ul)frames_per_buffer);
#else
    // Generic downmix
    local_persist f32 downmix_buf[INPUT_FRAMES_PER_BUFFER];
    ul frames = (ul)frames_per_buffer;
    if (frames > (ul)INPUT_FRAMES_PER_BUFFER)
    {
        frames = (ul)INPUT_FRAMES_PER_BUFFER;
    }

    for (ul i = 0; i < frames; i++)
    {
        f32 sum = 0.0f;
        for (i32 c = 0; c < INPUT_NUM_CHANNELS; c++)
        {
            sum += in[i * INPUT_NUM_CHANNELS + c];
        }

        downmix_buf[i] = sum / (f32)INPUT_NUM_CHANNELS;
    }
    mic_ring_push(app, downmix_buf, frames);
#endif

    return (int)paContinue;
}

i32 app_init_audio_capture(app_state_t *app_state)
{
    PaError err;
    err = Pa_Initialize();

    if (err != paNoError)
    {
        fprintf(stderr, "ERROR: PortAudio initialization error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    i32 num_devices = Pa_GetDeviceCount();
    if (num_devices < 0)
    {
        fprintf(stderr, "ERROR: Pa_GetDeviceCount returned 0x%x\n", num_devices);
        return 1;
    }

    printf("Available audio devices:\n");
    const PaDeviceInfo *device_info;
    for (i32 i = 0; i < num_devices; i++)
    {
        device_info = Pa_GetDeviceInfo(i);
        printf("  Device %d: %s\n", i, device_info->name);
    }

    // Allow selecting device via environment variable PA_DEVICE_INDEX, otherwise use default input
    i32 selected_device_index = Pa_GetDefaultInputDevice();
    const char *env = getenv("PA_DEVICE_INDEX");
    if (env)
    {
        i32 idx = atoi(env);
        if (idx >= 0 && idx < num_devices)
        {
            selected_device_index = idx;
        }
    }

    app_state->selected_device_info = (PaDeviceInfo *)Pa_GetDeviceInfo(selected_device_index);

    PaStreamParameters in_params;
    in_params.device = selected_device_index;
    in_params.channelCount = INPUT_NUM_CHANNELS;
    in_params.sampleFormat = paFloat32;
    in_params.suggestedLatency = app_state->selected_device_info->defaultLowInputLatency;
    in_params.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(
        &app_state->selected_device_stream,
        &in_params,
        NULL,
        (f64)INPUT_SAMPLE_RATE,
        (ul)INPUT_FRAMES_PER_BUFFER,
        paClipOff,
        audio_callback,
        app_state);

    if (err != paNoError)
    {
        printf("PortAudio stream error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    // Allocate ~2 seconds of ring buffer for mic capture
    app_state->mic_ring_capacity = (ul)(INPUT_SAMPLE_RATE * 2);
    app_state->mic_ring = (f32 *)calloc(app_state->mic_ring_capacity, sizeof(f32));
    app_state->mic_ring_write = 0;
    app_state->mic_ring_read = 0;
    memset(app_state->mic_window, 0, sizeof(app_state->mic_window));

    err = Pa_StartStream(app_state->selected_device_stream);
    if (err != paNoError)
    {
        printf("PortAudio start stream error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    return 0;
}

// Nuklear <-> Raylib minimal binding ---------------------------------------------
internal f32
nk_raylib_text_width(nk_handle handle, f32 h, const char *text, int len)
{
    if (!text || len <= 0)
    {
        return 0.0f;
    }

    Font *font = (Font *)handle.ptr;

    // Nuklear passes non-null-terminated strings
    char buf[512];
    if (len >= (int)sizeof(buf))
    {
        len = (int)sizeof(buf) - 1;
    }

    memcpy(buf, text, (size_t)len);
    buf[len] = '\0';
    Vector2 sz = MeasureTextEx(*font, buf, h, 0.0f);
    return sz.x;
}

internal Color
nk_to_ray(const struct nk_color c)
{
    return (Color){c.r, c.g, c.b, c.a};
}

internal void
nk_raylib_render(struct nk_context *ctx, Font *font)
{
    const struct nk_command *cmd;
    i32 scissor_active = 0;

    nk_foreach(cmd, ctx)
    {
        switch (cmd->type)
        {
        case NK_COMMAND_SCISSOR:
        {
            const struct nk_command_scissor *s = (const struct nk_command_scissor *)cmd;
            if (scissor_active)
            {
                EndScissorMode();
                scissor_active = 0;
            }

            BeginScissorMode((int)s->x, (int)s->y, (int)s->w, (int)s->h);
            scissor_active = 1;
        }
        break;
        case NK_COMMAND_RECT:
        {
            const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
            DrawRectangleLinesEx((Rectangle){(f64)r->x, (f64)r->y, (f64)r->w, (f64)r->h}, (f64)r->line_thickness, nk_to_ray(r->color));
        }
        break;
        case NK_COMMAND_RECT_FILLED:
        {
            const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
            DrawRectangleRec((Rectangle){(f64)r->x, (f64)r->y, (f64)r->w, (f64)r->h}, nk_to_ray(r->color));
        }
        break;
        case NK_COMMAND_TEXT:
        {
            const struct nk_command_text *t = (const struct nk_command_text *)cmd;

            // Nuklear text is not null-terminated
            char buf[1024];
            i32 n = t->length;
            if (n >= (int)sizeof(buf))
            {
                n = (int)sizeof(buf) - 1;
            }

            memcpy(buf, t->string, (size_t)n);
            buf[n] = '\0';
            DrawTextEx(*font, buf, (Vector2){(f64)t->x, (f64)t->y}, t->height, 0, nk_to_ray(t->foreground));
        }
        break;
        case NK_COMMAND_LINE:
        {
            const struct nk_command_line *l = (const struct nk_command_line *)cmd;
            DrawLineEx((Vector2){(f64)l->begin.x, (f64)l->begin.y},
                       (Vector2){(f64)l->end.x, (f64)l->end.y},
                       (f64)l->line_thickness, nk_to_ray(l->color));
        }
        break;
        default:
            break;
        }
    }

    if (scissor_active)
    {
        EndScissorMode();
    }

    nk_clear(ctx);
}

internal void
app_gui_init(app_state_t *app)
{
    // Bind Nuklear to our Raylib font via width callback
    local_persist struct nk_user_font nk_font;
    nk_font.userdata.ptr = &app->main_font;
    nk_font.height = 22.0f;
    nk_font.width = nk_raylib_text_width;

    nk_init_default(&app->nk, &nk_font);
    app->show_settings = 0;
}

internal void
app_gui_shutdown(app_state_t *app)
{
    nk_free(&app->nk);
    memset(&app->nk, 0, sizeof(app->nk));
}

internal void
app_gui_frame(app_state_t *app)
{
    struct nk_context *ctx = &app->nk;

    // Input
    nk_input_begin(ctx);
    Vector2 mp = GetMousePosition();
    nk_input_motion(ctx, (int)mp.x, (int)mp.y);
    i32 lmb = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)mp.x, (int)mp.y, lmb);
    f64 scroll = GetMouseWheelMove();
    if (scroll != 0.0f)
    {
        nk_input_scroll(ctx, (struct nk_vec2){0, scroll});
    }
    nk_input_end(ctx);

    // UI
    const i32 sw = GetScreenWidth();
    const i32 sh = GetScreenHeight();

    // Top-right small window with a Settings button
    const f64 tb_w = 120.0f, tb_h = 44.0f;
    const f64 tb_x = (f64)sw - tb_w - 16.0f;
    const f64 tb_y = 12.0f;
    if (nk_begin(ctx, "TopBar", nk_rect(tb_x, tb_y, tb_w, tb_h),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER))
    {
        nk_layout_row_dynamic(ctx, tb_h - 12.0f, 1);
        if (nk_button_label(ctx, "Settings"))
        {
            app->show_settings = 1;
        }
    }
    nk_end(ctx);

    if (app->show_settings)
    {
        const f64 mw = 520.0f, mh = 320.0f;
        const f64 mx = (sw - mw) * 0.5f;
        const f64 my = (sh - mh) * 0.5f;

        i32 open = nk_begin_titled(ctx, "SettingsModal", "Settings",
                                   nk_rect(mx, my, mw, mh),
                                   NK_WINDOW_MOVABLE | NK_WINDOW_BORDER |
                                       NK_WINDOW_TITLE | NK_WINDOW_CLOSABLE);
        if (open)
        {
            nk_layout_row_dynamic(ctx, 28.0f, 1);
            nk_label(ctx, "Audio Input Device Selected:", NK_TEXT_LEFT);

            nk_layout_row_dynamic(ctx, 36.0f, 1);
            if (nk_button_label(ctx, "Close"))
            {
                app->show_settings = 0;
            }
        }
        else
        {
            app->show_settings = 0;
        }
        nk_end(ctx);
    }

    nk_raylib_render(ctx, &app->main_font);
}

// end Nuklear <-> Raylib minimal binding ---------------------------------------------

i32 app_platform_init(app_state_t *app_state)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);
    SetWindowIcon(LoadImage("assets/icon.png"));

    app_state->main_font = LoadFontEx("assets/fonts/Roboto_Mono/RobotoMono-Regular.ttf", 64, 0, 256);
    app_state->windowed_w = WINDOW_WIDTH;
    app_state->windowed_h = WINDOW_HEIGHT;
    app_state->fractional_octave_index_selected = 4; // Default to 1/24 octave
    TraceLog(LOG_INFO, "Keys: O=Frac octave, P=Pink comp, A=dB avg, F=Avg preset, H=Peak hold");

    if (app_init_audio_capture(app_state) != 0)
    {
        fprintf(stderr, "ERROR: Failed to initialize audio capture\n");
        return 1;
    }

    app_gui_init(app_state);

    return 0;
}

i32 app_load_audio_data(app_state_t *app_state, const char *input_file)
{
    app_state->wave = LoadWave(input_file);
    if (app_state->wave.frameCount == 0)
    {
        fprintf(stderr, "Failed to load WAV: %s\n", input_file);
        app_cleanup(app_state);
        return 1;
    }

    app_state->sound = LoadSound(input_file);
    if (app_state->sound.frameCount == 0)
    {
        fprintf(stderr, "Failed to load sound: %s\n", input_file);
        app_cleanup(app_state);
        return 1;
    }

    app_state->samples = LoadWaveSamples(app_state->wave);
    if (!app_state->samples)
    {
        fprintf(stderr, "Failed to load wave samples: %s\n", input_file);
        app_cleanup(app_state);
        return 1;
    }

    return 0;
}

void app_run(app_state_t *app_state)
{
    if (!app_state->mic_mode)
    {
        PlaySound(app_state->sound);
    }

    app_state->running = true;

    while (!WindowShouldClose())
    {
        const f64 frame_dt = GetFrameTime();

        app_handle_input(app_state);

        spectrum_handle_resize(&app_state->spectrum_state);

        if (!app_state->mic_mode)
        {
            spectrum_update(&app_state->spectrum_state, &app_state->wave, app_state->samples, frame_dt);
            spectrum_render_to_texture(&app_state->spectrum_state);

            if (!app_state->loop_flag && spectrum_done(&app_state->spectrum_state))
            {
                break;
            }
            if (app_state->loop_flag && spectrum_done(&app_state->spectrum_state))
            {
                app_state->spectrum_state.window_index = 0;
                app_state->spectrum_state.accumulator = 0.0;
            }
            if (app_state->loop_flag && !IsSoundPlaying(app_state->sound))
            {
                PlaySound(app_state->sound);
            }
        }
        else
        {
            spectrum_state_t *s = &app_state->spectrum_state;

            ul avail = mic_ring_count(app_state);
            i32 hop = s->hop_size;
            ul max_windows_this_frame = avail / (ul)hop;

            // Cap to avoid long catch-up bursts
            if (max_windows_this_frame > 8)
            {
                max_windows_this_frame = 8;
            }

            Wave live_wave;
            live_wave.channels = 1;
            live_wave.sampleRate = INPUT_SAMPLE_RATE;
            live_wave.frameCount = FFT_WINDOW_SIZE;
            live_wave.data = NULL; // not used

            // Ensure we have an initial window filled once (prefer real data if available)
            local_persist i32 mic_initialized = 0;
            if (!mic_initialized)
            {
                ul need = (ul)FFT_WINDOW_SIZE;
                ul got = mic_ring_pop(app_state, app_state->mic_window, need);
                if (got < need)
                {
                    memset(app_state->mic_window + got, 0, (need - got) * sizeof(f32));
                }

                mic_initialized = 1;
            }

            // Process all whole windows we can form this frame
            for (ul w = 0; w < max_windows_this_frame; w++)
            {
                // Shift left by hop
                memmove(app_state->mic_window,
                        app_state->mic_window + hop,
                        (FFT_WINDOW_SIZE - hop) * sizeof(f32));

                // Append hop new samples (pad with zeros if short)
                ul got = mic_ring_pop(app_state,
                                      app_state->mic_window + (FFT_WINDOW_SIZE - hop),
                                      (ul)hop);
                if (got < (ul)hop)
                {
                    memset(app_state->mic_window + (FFT_WINDOW_SIZE - hop) + got, 0,
                           (size_t)((ul)hop - got) * sizeof(f32));
                }

                // One-window update: present current mic_window as the "sample buffer"
                spectrum_set_total_windows(s, 1);

                // Feed exactly one window worth of "time" so the internal while() runs once
                spectrum_update(s, &live_wave, app_state->mic_window, s->seconds_per_window);
            }

            // Feed remainder time into smoothing/meters without creating a new window
            f64 processed_time = (f64)max_windows_this_frame * s->seconds_per_window;
            f64 remainder = frame_dt - processed_time;
            if (remainder > 0.0)
            {
                spectrum_set_total_windows(s, 1);

                // With remainder < seconds_per_window, no new window will be computed
                spectrum_update(s, &live_wave, app_state->mic_window, remainder);
            }

            spectrum_render_to_texture(&app_state->spectrum_state);
        }

        BeginDrawing();
        ClearBackground(BLACK);
        render_draw(&app_state->spectrum_state);

        app_gui_frame(app_state);

        EndDrawing();
    }

    app_state->running = false;
}

void app_cleanup(app_state_t *app_state)
{
    if (app_state->main_font.texture.id != 0)
    {
        UnloadFont(app_state->main_font);
    }

    if (app_state->samples)
    {
        UnloadWaveSamples(app_state->samples);
    }

    if (app_state->wave.data)
    {
        UnloadWave(app_state->wave);
    }

    if (app_state->sound.stream.buffer)
    {
        StopSound(app_state->sound);
        UnloadSound(app_state->sound);
    }

    if (app_state->selected_device_stream)
    {
        Pa_StopStream(app_state->selected_device_stream);
        Pa_CloseStream(app_state->selected_device_stream);
        app_state->selected_device_stream = NULL;
    }
    Pa_Terminate();

    if (app_state->mic_ring)
    {
        free(app_state->mic_ring);
        app_state->mic_ring = NULL;
    }

    // Shutdown GUI
    app_gui_shutdown(app_state);

    spectrum_destroy(&app_state->spectrum_state);
    CloseAudioDevice();
    CloseWindow();

    if (app_state)
    {
        free(app_state);
    }
}
