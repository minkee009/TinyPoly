#include "app/app.h"
#include "app/window.h"

int main(void) {
    if (!WindowOpen()) return 1;

    static App app;
    AppInit(&app);
    while (!WindowShouldClose()) {
        AppUpdate(&app);
        AppDraw(&app);
    }

    CloseWindow();
    AppShutdown(&app);
    return 0;
}
