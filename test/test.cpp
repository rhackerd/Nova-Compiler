#include <cstdint>
#include <cstdio>
#include <raylib.h>

// extern "C" int init(uint64_t w, uint64_t h) {
//     SetConfigFlags(FLAG_WINDOW_RESIZABLE);
//     InitWindow(w, h, "Test");
//     return 0;
// }

// extern "C" int DrawRect(int posX, int posY, int width, int height) {
//     DrawRectangle(posX, posY, width, height, BLACK);
//     return 0;
// }

extern "C" bool DrawBg(int r, int g, int b) {
    ClearBackground({static_cast<unsigned char>(r),static_cast<unsigned char>(g),static_cast<unsigned char>(b),255});
    return true;
}

extern "C" bool DrawRect(int posX, int posY, int width, int height) {
    DrawRectangle(posX, posY, width, height, BLACK);
    return true;
}

extern "C" int MakeWindow() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "Test");
    return 0;
}


// extern "C" int test(int, int);
// extern "C" int draw();

// int main() {
//     test(10, 10);
//     while (!WindowShouldClose()) {
//         BeginDrawing();
//         ClearBackground(RAYWHITE);
//         draw();
//         EndDrawing();
//     }
//     CloseWindow();
//     return 0;
// }