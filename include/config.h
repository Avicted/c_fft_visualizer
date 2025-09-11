#ifndef CONFIG_H
#define CONFIG_H

#include <math.h>

#define M_PI acos(-1.0)

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720
#define FFT_WINDOW_SIZE 4096
#define FFT_HOP_SIZE (FFT_WINDOW_SIZE / 4)
#define HPF_CUTOFF_HZ 1.0
#define SMOOTH_ATTACK_MS 2.0
#define SMOOTH_RELEASE_MS 100.0
#define BAR_PIXEL_WIDTH 4
#define BAR_GAP 1
#define DB_TOP 0.0
#define DB_BOTTOM -60.0
#define DB_OFFSET 16.0
#define PEAK_DECAY_DB_PER_SEC 3.0
#define LERP_SPEED 0.10
#define EPSILON_POWER 1e-12

#define GRID_COLOR (Color){40, 40, 40, 96}

#define MARGIN_LEFT 60
#define MARGIN_RIGHT 20
#define MARGIN_TOP 64
#define MARGIN_BOTTOM 48

#endif
