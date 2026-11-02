#include "app/app.h"
#include "app/icon.h"
#include <stdio.h>

/* 창이 화면보다 크면 macOS가 몰래 줄이는데 raylib은 이를 몰라서 위쪽이 잘린다.
   모니터에 맞는 크기로 직접 줄여 resize 콜백이 오게 한다 */
static void fit_window_to_monitor(void) {
    int mon = GetCurrentMonitor();
    int mw = GetMonitorWidth(mon), mh = GetMonitorHeight(mon);
    int w = GetScreenWidth(), h = GetScreenHeight();
    if (mw > 0 && w > mw * 9 / 10) w = mw * 9 / 10;
    if (mh > 0 && h > mh * 8 / 10) h = mh * 8 / 10;
    if (w < 800) w = 800;
    if (h < 500) h = 500;
    if (w != GetScreenWidth() || h != GetScreenHeight()) {
        SetWindowSize(w, h);
        /* 보조 모니터에서도 그 모니터 가운데로 (모니터 위치를 더함) */
        Vector2 mp = GetMonitorPosition(mon);
        SetWindowPosition((int)mp.x + (mw - w) / 2, (int)mp.y + (mh - h) / 2);
    }
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 800, "TinyPoly");
    if (!IsWindowReady()) {
        fprintf(stderr, "InitWindow failed\n");
        return 1;
    }
    SetWindowMinSize(800, 500);
    app_set_icon();
    fit_window_to_monitor();
    SetExitKey(KEY_NULL);
    HideCursor();                   /* 창 안에서는 도트 커서를 직접 그림 (cursor.c) */

    static App app;
    app_init(&app);
    while (!WindowShouldClose()) {
        app_update(&app);
        app_draw(&app);
    }

    CloseWindow();
    app_shutdown(&app);
    return 0;
}
