#include "texview.h"
#include <raymath.h>
#include <math.h>

static const float ZOOMS[] = { 0.125f, 0.25f, 0.5f, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
#define NZOOM ((int)(sizeof ZOOMS / sizeof ZOOMS[0]))

static int tex_w(const App *a) { return a->scene.tex->w; }
static int tex_h(const App *a) { return a->scene.tex->h; }

Rectangle texview_area(const App *a) {
    Rectangle c = a->canvas_rect;
    return (Rectangle){ c.x, c.y + TOOLBAR_H, c.width, c.height - TOOLBAR_H - PALETTE_BAR_H };
}

/* 창에 맞춤: 확대 단계 중 화면에 다 들어가는 가장 큰 값 */
static float fit_scale(const App *a) {
    Rectangle r = texview_area(a);
    float raw = fminf((r.width - 16) / tex_w(a), (r.height - 16) / tex_h(a));
    float best = ZOOMS[0];
    for (int i = 0; i < NZOOM; i++)
        if (ZOOMS[i] <= raw) best = ZOOMS[i];
    return best;
}

float texview_scale(const App *a) {
    return a->tex_zoom > 0 ? a->tex_zoom : fit_scale(a);
}

Vector2 texview_origin(const App *a) {
    Rectangle r = texview_area(a);
    float sc = texview_scale(a);
    return (Vector2){ floorf(r.x + r.width / 2 - tex_w(a) * sc / 2 + a->tex_pan.x),
                      floorf(r.y + r.height / 2 - tex_h(a) * sc / 2 + a->tex_pan.y) };
}

Vector2 texview_to_tex(const App *a, Vector2 m) {
    Vector2 o = texview_origin(a);
    float sc = texview_scale(a);
    return (Vector2){ (m.x - o.x) / sc, (m.y - o.y) / sc };
}

Vector2 texview_to_screen(const App *a, Vector2 t) {
    Vector2 o = texview_origin(a);
    float sc = texview_scale(a);
    return (Vector2){ o.x + t.x * sc, o.y + t.y * sc };
}

bool texview_texel(const App *a, int x, int y, Color *out) {
    if (x < 0 || y < 0 || x >= tex_w(a) || y >= tex_h(a)) return false;
    *out = a->scene.tex->px[y * tex_w(a) + x];
    return true;
}

/* 확대: 마우스 아래 텍셀이 그 자리에 머물도록 이동량을 맞춤 */
void texview_zoom_to(App *a, float z, Vector2 anchor) {
    Vector2 t = texview_to_tex(a, anchor);
    a->tex_zoom = z;
    Vector2 p = texview_to_screen(a, t);
    a->tex_pan = Vector2Add(a->tex_pan, Vector2Subtract(anchor, p));
}

void texview_zoom_step(App *a, int dir, Vector2 anchor) {
    float cur = texview_scale(a);
    int i = 0;
    while (i < NZOOM - 1 && ZOOMS[i] < cur) i++;
    if (dir > 0 && ZOOMS[i] <= cur && i < NZOOM - 1) i++;
    if (dir < 0 && i > 0) i--;
    texview_zoom_to(a, ZOOMS[i], anchor);
}

void texview_zoom_fit(App *a) {
    a->tex_zoom = 0;
    a->tex_pan = (Vector2){ 0 };
}
