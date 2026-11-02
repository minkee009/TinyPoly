#include "paint/palbar.h"
#include "paint/texview.h"
#include "ui/ui.h"
#include <raymath.h>

#define SWATCH 22

static Rectangle palbar_rect(const App *a) {
    Rectangle c = a->canvas_rect;
    return (Rectangle){ c.x, c.y + c.height - PALETTE_BAR_H, c.width, PALETTE_BAR_H };
}

/* 팔레트 칸 배치: [투명][팔레트 0..n-1][+] 를 두 줄로, 넘치면 pal_scroll 줄부터 */
static int pal_cols(const App *a) {
    Rectangle r = palbar_rect(a);
    int cols = (int)((r.width - 80 - 110) / SWATCH);
    return cols < 4 ? 4 : cols;
}

static Rectangle pal_cell(const App *a, int slot) {
    Rectangle r = palbar_rect(a);
    int cols = pal_cols(a);
    int row = slot / cols - a->pal_scroll, col = slot % cols;
    return (Rectangle){ r.x + 80 + col * SWATCH, r.y + 6 + row * SWATCH, SWATCH - 2, SWATCH - 2 };
}

void palbar_update(App *a) {
    Rectangle r = palbar_rect(a);
    Vector2 m = GetMousePosition();
    if (!CheckCollisionPointRec(m, r) || a->ui_hover) return;

    int cols = pal_cols(a), slots = a->scene.npalette + 2;
    int rows = (slots + cols - 1) / cols;
    float wheel = GetMouseWheelMove();
    if (wheel != 0) a->pal_scroll = (int)Clamp((float)a->pal_scroll - (wheel > 0 ? 1 : -1), 0, (float)(rows > 2 ? rows - 2 : 0));

    /* 왼쪽 큰 칸 두 개: 누르면 색 선택 창 */
    Rectangle c1 = { r.x + 8, r.y + 8, 36, 36 }, c2 = { r.x + 30, r.y + 22, 36, 36 };
    bool l = IsMouseButtonPressed(MOUSE_BUTTON_LEFT), rt = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (l && CheckCollisionPointRec(m, c1)) { a->pick_target = 0; a->show_color = 1; return; }
    if (l && CheckCollisionPointRec(m, c2)) { a->pick_target = 1; a->show_color = 1; return; }

    for (int s = 0; s < slots; s++) {
        Rectangle cell = pal_cell(a, s);
        if (cell.y < r.y || cell.y + cell.height > r.y + r.height || !CheckCollisionPointRec(m, cell)) continue;
        if (s == slots - 1) {
            /* + : 지금 색을 팔레트에 추가 */
            if (l && a->scene.npalette < MAX_PALETTE) {
                app_mark(a);
                a->scene.palette[a->scene.npalette++] = a->color;
                a->pal_sel = a->scene.npalette - 1;
            }
            return;
        }
        Color c = s == 0 ? BLANK : a->scene.palette[s - 1];
        if (l) { a->color = c; a->pal_sel = s - 1; }
        if (rt) a->color2 = c;
        return;
    }
}

/* 투명이면 체크무늬 위에 색 */
static void draw_swatch(Rectangle r, Color c) {
    if (c.a < 255) ui_checker(r, 5);
    DrawRectangleRec(r, c);
}

void palbar_draw(App *a) {
    Rectangle r = palbar_rect(a);
    DrawRectangleRec(r, (Color){ 30, 28, 40, 255 });

    /* 지금 색 두 개 (뒤가 color2) */
    Rectangle c1 = { r.x + 8, r.y + 8, 36, 36 }, c2 = { r.x + 30, r.y + 22, 36, 36 };
    draw_swatch(c2, a->color2);
    DrawRectangleLinesEx(c2, 2, (Color){ 20, 18, 28, 255 });
    draw_swatch(c1, a->color);
    DrawRectangleLinesEx(c1, 2, WHITE);

    BeginScissorMode((int)r.x, (int)r.y + 4, (int)r.width, (int)r.height - 8);
    int slots = a->scene.npalette + 2;
    for (int s = 0; s < slots; s++) {
        Rectangle cell = pal_cell(a, s);
        if (s == slots - 1) {
            DrawRectangleLinesEx(cell, 1, (Color){ 120, 116, 150, 255 });
            DrawTextEx(GetFontDefault(), "+", (Vector2){ cell.x + 5, cell.y }, 20, 2, WHITE);
            continue;
        }
        Color c = s == 0 ? BLANK : a->scene.palette[s - 1];
        draw_swatch(cell, c);
        if (s - 1 == a->pal_sel && s > 0) DrawRectangleLinesEx(cell, 2, WHITE);
    }
    EndScissorMode();

    if (ui_button_rect((Rectangle){ r.x + r.width - 104, r.y + 14, 96, 32 }, "color (P)", a->show_color, a->ui_hover))
        a->show_color = !a->show_color;
}
