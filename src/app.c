#include "app.h"
#include <math.h>
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
            "  W   Frequency weighting (Z/A/C)\n"
            "  T   Time weighting (Fast/Slow/Impulse)\n"
            "  K   Calibrate SPL to 94 dB (mic mode only)\n"
            "  G   Peak-find (max-hold)\n"
            "  Left/Right  Step locked band\n"
            "  Mouse Left  Toggle nearest-band lock\n"
            "  R   Reset peaks/max-hold\n"
            "  Space Pause/Resume (file) or Freeze (mic)\n"
            "  F11 Fullscreen\n",
            prog, prog);
}

internal const char *
freq_weight_label(i32 mode)
{
    if (mode == FREQ_WEIGHTING_A)
    {
        return "A";
    }
    if (mode == FREQ_WEIGHTING_C)
    {
        return "C";
    }

    return "Z";
}

internal const char *
time_weight_label(i32 mode)
{
    if (mode == TIME_WEIGHTING_SLOW)
    {
        return "Slow";
    }
    if (mode == TIME_WEIGHTING_IMPULSE)
    {
        return "Impulse";
    }

    return "Fast";
}

internal i32
clamp_bar_index(const spectrum_state_t *s, i32 index)
{
    if (s->num_bars <= 0)
    {
        return -1;
    }

    if (index < 0)
    {
        return 0;
    }
    if (index >= s->num_bars)
    {
        return s->num_bars - 1;
    }

    return index;
}

internal i32
cursor_index_from_mouse(const spectrum_state_t *s)
{
    if (s->num_bars <= 0)
    {
        return -1;
    }

    Vector2 mouse = GetMousePosition();
    i32 mx = (i32)mouse.x;
    i32 my = (i32)mouse.y;
    if (mx < s->plot_left || mx >= s->plot_left + s->plot_width ||
        my < s->plot_top || my >= s->plot_top + s->plot_height)
    {
        return -1;
    }

    i32 stride = BAR_PIXEL_WIDTH + BAR_GAP;
    i32 index = (mx - s->plot_left) / stride;
    return clamp_bar_index(s, index);
}

internal void
app_sync_cursor_indices(app_state_t *app_state)
{
    spectrum_state_t *s = &app_state->spectrum_state;
    app_state->cursor_hover_index = cursor_index_from_mouse(s);

    if (app_state->cursor_lock_enabled)
    {
        app_state->cursor_locked_index = clamp_bar_index(s, app_state->cursor_locked_index);
        if (app_state->cursor_locked_index < 0)
        {
            app_state->cursor_lock_enabled = 0;
        }
    }
}

internal i32
find_max_hold_peak_index(const spectrum_state_t *s)
{
    if (s->num_bars <= 0 || !s->max_hold_power)
    {
        return -1;
    }

    i32 best_index = 0;
    f64 best_power = s->max_hold_power[0];
    for (i32 i = 1; i < s->num_bars; i++)
    {
        if (s->max_hold_power[i] > best_power)
        {
            best_power = s->max_hold_power[i];
            best_index = i;
        }
    }

    if (best_power <= 0.0)
    {
        return -1;
    }

    return best_index;
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
        app_sync_cursor_indices(app_state);
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
        app_sync_cursor_indices(app_state);
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

    if (IsKeyPressed(KEY_W))
    {
        spectrum_state_t *s = &app_state->spectrum_state;
        spectrum_cycle_frequency_weighting(s);
        TraceLog(LOG_INFO, "Frequency weighting: %s", freq_weight_label(s->frequency_weighting_mode));
    }

    if (IsKeyPressed(KEY_T))
    {
        spectrum_state_t *s = &app_state->spectrum_state;
        spectrum_cycle_time_weighting(s);
        TraceLog(LOG_INFO, "Time weighting: %s", time_weight_label(s->time_weighting_mode));
    }

    if (IsKeyPressed(KEY_K))
    {
        spectrum_state_t *s = &app_state->spectrum_state;
        if (!s->spl_features_enabled)
        {
            TraceLog(LOG_INFO, "SPL calibration is disabled in file input mode");
        }
        else
        {
            spectrum_calibrate_spl(s, s->calibrator_target_db_spl);

            if (s->spl_calibrated)
            {
                TraceLog(LOG_INFO, "SPL calibrated: target %.1f dB SPL, offset %+.1f dB", s->calibrator_target_db_spl, s->spl_offset_db);
            }
            else
            {
                TraceLog(LOG_WARNING, "SPL calibration skipped: no valid RMS reading yet");
            }
        }
    }

    if (IsKeyPressed(KEY_R))
    {
        spectrum_reset_peaks(&app_state->spectrum_state);
    }

    if (IsKeyPressed(KEY_G))
    {
        i32 peak_index = find_max_hold_peak_index(&app_state->spectrum_state);
        if (peak_index >= 0)
        {
            app_state->cursor_lock_enabled = 1;
            app_state->cursor_locked_index = peak_index;
        }
    }

    if (app_state->cursor_lock_enabled)
    {
        if (IsKeyPressed(KEY_LEFT))
        {
            app_state->cursor_locked_index = clamp_bar_index(&app_state->spectrum_state, app_state->cursor_locked_index - 1);
        }

        if (IsKeyPressed(KEY_RIGHT))
        {
            app_state->cursor_locked_index = clamp_bar_index(&app_state->spectrum_state, app_state->cursor_locked_index + 1);
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        i32 index = app_state->cursor_hover_index;
        if (index >= 0)
        {
            if (app_state->cursor_lock_enabled && app_state->cursor_locked_index == index)
            {
                app_state->cursor_lock_enabled = 0;
            }
            else
            {
                app_state->cursor_lock_enabled = 1;
                app_state->cursor_locked_index = index;
            }
        }
    }

    if (IsKeyPressed(KEY_SPACE))
    {
        app_state->freeze_enabled ^= 1;

        if (!app_state->mic_mode)
        {
            if (app_state->freeze_enabled)
            {
                PauseMusicStream(app_state->music);
                SetWindowTitle("FFT Visualizer [PAUSED]");
            }
            else
            {
                ResumeMusicStream(app_state->music);
                SetWindowTitle("FFT Visualizer");
            }
        }
        else
        {
            if (app_state->freeze_enabled)
            {
                SetWindowTitle("FFT Visualizer [FROZEN]");
            }
            else
            {
                SetWindowTitle("FFT Visualizer");
            }
        }
    }

    app_sync_cursor_indices(app_state);
}

internal inline ul
mic_ring_count_unlocked(const app_state_t *app)
{
    return app->mic_ring_write - app->mic_ring_read;
}

internal inline ul
mic_ring_space_unlocked(const app_state_t *app)
{
    return app->mic_ring_capacity - mic_ring_count_unlocked(app);
}

internal ul
mic_ring_count(app_state_t *app)
{
    ul count;
    if (!app->mic_ring_mutex_initialized)
    {
        return 0;
    }

    pthread_mutex_lock(&app->mic_ring_mutex);
    count = mic_ring_count_unlocked(app);
    pthread_mutex_unlock(&app->mic_ring_mutex);
    return count;
}

internal ul
mic_ring_push(app_state_t *app, const f32 *src, ul n)
{
    if (!app->mic_ring_mutex_initialized)
    {
        return 0;
    }

    pthread_mutex_lock(&app->mic_ring_mutex);

    ul space = mic_ring_space_unlocked(app);
    if (n > space)
    {
        app->mic_ring_dropped_frames += (n - space);
        n = space; // drop overflow
    }

    ul cap = app->mic_ring_capacity;
    ul w = app->mic_ring_write;
    for (ul i = 0; i < n; i++)
    {
        app->mic_ring[(w + i) % cap] = src[i];
    }

    app->mic_ring_write = w + n;
    pthread_mutex_unlock(&app->mic_ring_mutex);
    return n;
}

internal ul
mic_ring_pop(app_state_t *app, f32 *dst, ul n)
{
    if (!app->mic_ring_mutex_initialized)
    {
        return 0;
    }

    pthread_mutex_lock(&app->mic_ring_mutex);

    ul avail = mic_ring_count_unlocked(app);
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
    pthread_mutex_unlock(&app->mic_ring_mutex);
    return n;
}

internal ul
mic_ring_discard_all(app_state_t *app)
{
    ul dropped = 0;
    if (!app->mic_ring_mutex_initialized)
    {
        return 0;
    }

    pthread_mutex_lock(&app->mic_ring_mutex);
    dropped = mic_ring_count_unlocked(app);
    app->mic_ring_read = app->mic_ring_write;
    pthread_mutex_unlock(&app->mic_ring_mutex);
    return dropped;
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
        for (int c = 0; c < INPUT_NUM_CHANNELS; c++)
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
    if (app_state->selected_device_stream)
    {
        return 0;
    }

    PaError err;
    err = Pa_Initialize();

    if (err != paNoError)
    {
        fprintf(stderr, "ERROR: PortAudio initialization error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    int num_devices = Pa_GetDeviceCount();
    if (num_devices < 0)
    {
        fprintf(stderr, "ERROR: Pa_GetDeviceCount returned 0x%x\n", num_devices);
        return 1;
    }

    printf("Available audio devices:\n");
    const PaDeviceInfo *device_info;
    for (int i = 0; i < num_devices; i++)
    {
        device_info = Pa_GetDeviceInfo(i);
        printf("  Device %d: %s\n", i, device_info->name);
    }

    // Allow selecting device via environment variable PA_DEVICE_INDEX, otherwise use default input
    i32 selected_device_index = Pa_GetDefaultInputDevice();
    if (selected_device_index == paNoDevice)
    {
        fprintf(stderr, "ERROR: No default input audio device available\n");
        return 1;
    }

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

    f64 requested_sample_rate = app_state->selected_device_info->defaultSampleRate;
    if (requested_sample_rate <= 0.0)
    {
        requested_sample_rate = (f64)INPUT_SAMPLE_RATE;
    }

    err = Pa_OpenStream(
        &app_state->selected_device_stream,
        &in_params,
        NULL,
        requested_sample_rate,
        (ul)INPUT_FRAMES_PER_BUFFER,
        paClipOff,
        audio_callback,
        app_state);

    if (err != paNoError)
    {
        printf("PortAudio stream error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    const PaStreamInfo *stream_info = Pa_GetStreamInfo(app_state->selected_device_stream);
    if (stream_info && stream_info->sampleRate > 0.0)
    {
        app_state->input_sample_rate = stream_info->sampleRate;
    }
    else
    {
        app_state->input_sample_rate = (f64)INPUT_SAMPLE_RATE;
    }

    if (pthread_mutex_init(&app_state->mic_ring_mutex, NULL) != 0)
    {
        fprintf(stderr, "ERROR: Failed to initialize mic ring mutex\n");
        return 1;
    }
    app_state->mic_ring_mutex_initialized = 1;

    // Allocate ~2 seconds of ring buffer for mic capture
    app_state->mic_ring_capacity = (ul)(app_state->input_sample_rate * 2.0);
    if (app_state->mic_ring_capacity < (ul)(FFT_WINDOW_SIZE * 2))
    {
        app_state->mic_ring_capacity = (ul)(FFT_WINDOW_SIZE * 2);
    }

    app_state->mic_ring = (f32 *)calloc(app_state->mic_ring_capacity, sizeof(f32));
    if (!app_state->mic_ring)
    {
        fprintf(stderr, "ERROR: Failed to allocate mic ring buffer\n");
        return 1;
    }

    app_state->mic_ring_write = 0;
    app_state->mic_ring_read = 0;
    app_state->mic_ring_dropped_frames = 0;
    memset(app_state->mic_window, 0, sizeof(app_state->mic_window));

    err = Pa_StartStream(app_state->selected_device_stream);
    if (err != paNoError)
    {
        printf("PortAudio start stream error: %s\n", Pa_GetErrorText(err));
        return 1;
    }

    return 0;
}

i32 app_platform_init(app_state_t *app_state)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    SetAudioStreamBufferSizeDefault(AUDIO_STREAM_BUFFER_SAMPLES);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);
    SetWindowIcon(LoadImage("assets/icon.png"));

    app_state->main_font = LoadFontEx("assets/fonts/Roboto_Mono/RobotoMono-Regular.ttf", 120, 0, 250);
    app_state->windowed_w = WINDOW_WIDTH;
    app_state->windowed_h = WINDOW_HEIGHT;
    app_state->cursor_lock_enabled = 0;
    app_state->cursor_locked_index = -1;
    app_state->cursor_hover_index = -1;
    app_state->input_sample_rate = (f64)INPUT_SAMPLE_RATE;
    app_state->fractional_octave_index_selected = 4; // Default to 1/24 octave
    TraceLog(LOG_INFO, "Keys: O=Frac octave, P=Pink comp, A=dB avg, F=Avg preset, H=Peak hold, W=Weighting, T=Time weighting, K=Calibrate, G=Peak-find, Arrows=Step lock, Click=Toggle lock, R=Reset peaks/max-hold, Space=Pause/Resume (file) or Freeze (mic)");

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

    app_state->music = LoadMusicStream(input_file);
    if (app_state->music.stream.buffer == NULL)
    {
        fprintf(stderr, "Failed to load music stream: %s\n", input_file);
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
    f64 playback_analysis_accum = 0.0;
    f64 *interp_from_bar = NULL;
    f64 *interp_to_bar = NULL;
    f64 *interp_curr_bar = NULL;
    f64 *interp_from_peak = NULL;
    f64 *interp_to_peak = NULL;
    f64 *interp_curr_peak = NULL;
    i32 interp_bar_count = 0;
    i32 interp_ready = 0;

    if (!app_state->mic_mode)
    {
        app_state->music.looping = (app_state->loop_flag != 0);
        PlayMusicStream(app_state->music);
        app_state->playback_time_prev = 0.0;
    }

    app_state->running = true;

    while (!WindowShouldClose())
    {
        const f64 frame_dt = GetFrameTime();

        app_handle_input(app_state);

        spectrum_handle_resize(&app_state->spectrum_state);
        app_sync_cursor_indices(app_state);

        if (!app_state->mic_mode)
        {
            spectrum_state_t *s = &app_state->spectrum_state;

            if (!app_state->freeze_enabled)
            {
                UpdateMusicStream(app_state->music);
            }

            playback_analysis_accum += frame_dt;

            f64 analysis_interval = 1.0 / PLAYBACK_ANALYSIS_FPS;
            if (analysis_interval <= 0.0)
            {
                analysis_interval = 1.0 / 30.0;
            }

            i32 run_analysis_now = (playback_analysis_accum >= analysis_interval);
            if (run_analysis_now)
            {
                while (playback_analysis_accum >= analysis_interval)
                {
                    playback_analysis_accum -= analysis_interval;
                }
            }

            if (s->num_bars > 0 && s->num_bars != interp_bar_count)
            {
                i32 n = s->num_bars;
                f64 *new_from_bar = (f64 *)calloc((size_t)n, sizeof(f64));
                f64 *new_to_bar = (f64 *)calloc((size_t)n, sizeof(f64));
                f64 *new_curr_bar = (f64 *)calloc((size_t)n, sizeof(f64));
                f64 *new_from_peak = (f64 *)calloc((size_t)n, sizeof(f64));
                f64 *new_to_peak = (f64 *)calloc((size_t)n, sizeof(f64));
                f64 *new_curr_peak = (f64 *)calloc((size_t)n, sizeof(f64));

                if (new_from_bar && new_to_bar && new_curr_bar &&
                    new_from_peak && new_to_peak && new_curr_peak)
                {
                    free(interp_from_bar);
                    free(interp_to_bar);
                    free(interp_curr_bar);
                    free(interp_from_peak);
                    free(interp_to_peak);
                    free(interp_curr_peak);

                    interp_from_bar = new_from_bar;
                    interp_to_bar = new_to_bar;
                    interp_curr_bar = new_curr_bar;
                    interp_from_peak = new_from_peak;
                    interp_to_peak = new_to_peak;
                    interp_curr_peak = new_curr_peak;
                    interp_bar_count = n;

                    for (i32 b = 0; b < n; b++)
                    {
                        f64 bar = s->bar_smoothed[b];
                        f64 peak = s->peak_power[b];
                        interp_from_bar[b] = bar;
                        interp_to_bar[b] = bar;
                        interp_curr_bar[b] = bar;
                        interp_from_peak[b] = peak;
                        interp_to_peak[b] = peak;
                        interp_curr_peak[b] = peak;
                    }

                    interp_ready = 1;
                }
                else
                {
                    free(new_from_bar);
                    free(new_to_bar);
                    free(new_curr_bar);
                    free(new_from_peak);
                    free(new_to_peak);
                    free(new_curr_peak);
                }
            }

            if (!app_state->freeze_enabled && run_analysis_now)
            {
                f64 playback_time_now = GetMusicTimePlayed(app_state->music);
                f64 playback_dt = playback_time_now - app_state->playback_time_prev;

                if (playback_dt < 0.0)
                {
                    // Stream looped (or restarted): reset FFT playback cursor
                    spectrum_set_total_windows(s, s->total_windows);
                    playback_dt = playback_time_now;
                }

                if (playback_dt > 0.0)
                {
                    spectrum_update(s, &app_state->wave, app_state->samples, playback_dt);
                }

                if (interp_ready)
                {
                    for (i32 b = 0; b < s->num_bars; b++)
                    {
                        interp_from_bar[b] = interp_curr_bar[b];
                        interp_to_bar[b] = s->bar_smoothed[b];
                        interp_from_peak[b] = interp_curr_peak[b];
                        interp_to_peak[b] = s->peak_power[b];
                    }
                }

                app_state->playback_time_prev = playback_time_now;
            }
            else if (app_state->freeze_enabled)
            {
                // If frozen, keep sync base current to avoid a large catch-up jump on unfreeze.
                app_state->playback_time_prev = GetMusicTimePlayed(app_state->music);
            }

            if (!app_state->freeze_enabled)
            {
                UpdateMusicStream(app_state->music);
            }

            if (interp_ready && !app_state->freeze_enabled)
            {
                f64 alpha = playback_analysis_accum / analysis_interval;
                if (alpha < 0.0)
                {
                    alpha = 0.0;
                }
                if (alpha > 1.0)
                {
                    alpha = 1.0;
                }

                for (i32 b = 0; b < s->num_bars; b++)
                {
                    interp_curr_bar[b] = interp_from_bar[b] + (interp_to_bar[b] - interp_from_bar[b]) * alpha;
                    interp_curr_peak[b] = interp_from_peak[b] + (interp_to_peak[b] - interp_from_peak[b]) * alpha;
                }

                f64 *saved_bars = s->bar_smoothed;
                f64 *saved_peaks = s->peak_power;
                s->bar_smoothed = interp_curr_bar;
                s->peak_power = interp_curr_peak;
                spectrum_render_to_texture(s);
                s->bar_smoothed = saved_bars;
                s->peak_power = saved_peaks;
            }
            else
            {
                spectrum_render_to_texture(s);
            }

            if (!app_state->loop_flag && !app_state->freeze_enabled && !IsMusicStreamPlaying(app_state->music))
            {
                break;
            }

            if (app_state->loop_flag && spectrum_done(&app_state->spectrum_state))
            {
                spectrum_set_total_windows(&app_state->spectrum_state, app_state->spectrum_state.total_windows);
            }
        }
        else
        {
            spectrum_state_t *s = &app_state->spectrum_state;

            if (app_state->freeze_enabled)
            {
                mic_ring_discard_all(app_state);
                spectrum_render_to_texture(&app_state->spectrum_state);

                BeginDrawing();
                ClearBackground(BLACK);
                render_draw(&app_state->spectrum_state,
                            app_state->cursor_lock_enabled,
                            app_state->cursor_locked_index,
                            app_state->cursor_hover_index,
                            0);
                EndDrawing();
                continue;
            }

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
            live_wave.sampleRate = (i32)app_state->input_sample_rate;
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
                    ul pad = need - got;
                    memmove(app_state->mic_window + pad, app_state->mic_window, got * sizeof(f32));
                    memset(app_state->mic_window, 0, pad * sizeof(f32));
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
        render_draw(&app_state->spectrum_state,
                    app_state->cursor_lock_enabled,
                    app_state->cursor_locked_index,
                    app_state->cursor_hover_index,
                    (!app_state->mic_mode && app_state->freeze_enabled));
        EndDrawing();
    }

    free(interp_from_bar);
    free(interp_to_bar);
    free(interp_curr_bar);
    free(interp_from_peak);
    free(interp_to_peak);
    free(interp_curr_peak);

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

    if (app_state->music.stream.buffer)
    {
        StopMusicStream(app_state->music);
        UnloadMusicStream(app_state->music);
    }

    if (app_state->selected_device_stream)
    {
        Pa_StopStream(app_state->selected_device_stream);
        Pa_CloseStream(app_state->selected_device_stream);
        app_state->selected_device_stream = NULL;
    }

    if (app_state->mic_ring_mutex_initialized)
    {
        pthread_mutex_destroy(&app_state->mic_ring_mutex);
        app_state->mic_ring_mutex_initialized = 0;
    }

    Pa_Terminate();

    if (app_state->mic_ring)
    {
        free(app_state->mic_ring);
        app_state->mic_ring = NULL;
    }

    spectrum_destroy(&app_state->spectrum_state);
    CloseAudioDevice();
    CloseWindow();

    if (app_state)
    {
        free(app_state);
    }
}
