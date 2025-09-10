#include "macros.h"

#include <raylib.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

int main(void)
{
    printf("Hello Sailor!\n");

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "FFT Visualizer");
    InitAudioDevice();
    SetTargetFPS(60);

    const char *input_file_path = "Firefly_in_a_fairytale_by_Gareth_Coker.wav";

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Hello, world!", 10, 10, 20, WHITE);
        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();

    return 0;
}
