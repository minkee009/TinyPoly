#include "paint/texview.h"
#include <raymath.h>
#include <math.h>

static const float ZOOMS[] = { 0.125f, 0.25f, 0.5f, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32 };
#define NZOOM ((int)(sizeof ZOOMS / sizeof ZOOMS[0]))

static int TexW(const App *a) { return a->scene.tex->w; }
static int TexH(const App *a) { return a->scene.tex->h; }

Rectangle TexViewArea(const App *a) {
    Rectangle c = a->canvas_rect;
    return (Rectangle){ c.x, c.y + TOOLBAR_H, c.width, c.height - TOOLBAR_H - PALETTE_BAR_H };
}

/* 창에 맞춤: 확대 단계 중 화면에 다 들어가는 가장 큰 값 */
static float FitScale(const App *a) {
    Rectangle r = TexViewArea(a);
    float raw = fminf((r.width - 16) / TexW(a), (r.height - 16) / TexH(a));
    float best = ZOOMS[0];
    for (int i = 0; i < NZOOM; i++)
        if (ZOOMS[i] <= raw) best = ZOOMS[i];
    return best;
}

float TexViewScale(const App *a) {
    return a->tex_zoom > 0 ? a->tex_zoom : FitScale(a);
}

Vector2 TexViewOrigin(const App *a) {
    Rectangle r = TexViewArea(a);
    float sc = TexViewScale(a);
    return (Vector2){ floorf(r.x + r.width / 2 - TexW(a) * sc / 2 + a->tex_pan.x),
                      floorf(r.y + r.height / 2 - TexH(a) * sc / 2 + a->tex_pan.y) };
}

Vector2 TexViewToTex(const App *a, Vector2 m) {
    Vector2 o = TexViewOrigin(a);
    float sc = TexViewScale(a);
    return (Vector2){ (m.x - o.x) / sc, (m.y - o.y) / sc };
}

Vector2 TexViewToScreen(const App *a, Vector2 t) {
    Vector2 o = TexViewOrigin(a);
    float sc = TexViewScale(a);
    return (Vector2){ o.x + t.x * sc, o.y + t.y * sc };
}

bool TexViewTexel(const App *a, int x, int y, Color *out) {
    if (x < 0 || y < 0 || x >= TexW(a) || y >= TexH(a)) return false;
    *out = a->scene.tex->px[y * TexW(a) + x];
    return true;
}

/* 확대: 마우스 아래 텍셀이 그 자리에 머물도록 이동량을 맞춤 */
void TexViewZoomTo(App *a, float z, Vector2 anchor) {
    Vector2 t = TexViewToTex(a, anchor);
    a->tex_zoom = z;
    Vector2 p = TexViewToScreen(a, t);
    a->tex_pan = Vector2Add(a->tex_pan, Vector2Subtract(anchor, p));
}

void TexViewZoomStep(App *a, int dir, Vector2 anchor) {
    float cur = TexViewScale(a);
    int i = 0;
    while (i < NZOOM - 1 && ZOOMS[i] < cur) i++;
    if (dir > 0 && ZOOMS[i] <= cur && i < NZOOM - 1) i++;
    if (dir < 0 && i > 0) i--;
    TexViewZoomTo(a, ZOOMS[i], anchor);
}

void TexViewZoomFit(App *a) {
    a->tex_zoom = 0;
    a->tex_pan = (Vector2){ 0 };
}
