#include "texture.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* 0~7 어두운 쪽, 8~15 밝은 쪽. 7이 거의 흰색 */
const Color DEFAULT_PALETTE[PALETTE_SIZE] = {
    {  20,  18,  28, 255 }, {  42,  46,  84, 255 }, { 118,  52, 102, 255 }, {  22,  96,  74, 255 },
    { 148,  86,  60, 255 }, {  88,  84,  92, 255 }, { 180, 178, 186, 255 }, { 248, 244, 236, 255 },
    { 230,  58,  70, 255 }, { 250, 152,  40, 255 }, { 252, 230,  70, 255 }, {  80, 210,  80, 255 },
    {  60, 160, 250, 255 }, { 132, 116, 190, 255 }, { 248, 128, 170, 255 }, { 252, 200, 164, 255 },
};

void scene_reset_palette(Scene *s) {
    memcpy(s->palette, DEFAULT_PALETTE, sizeof s->palette);
    s->bg = 1;
    s->alpha = 0;
    tex_auto_ramp(s);
}

int tex_shade_index(const Scene *s, int idx, int level) {
    for (int i = 0; i < level; i++) idx = s->ramp[idx];
    return idx;
}

Color tex_shade_color(const Scene *s, int idx, int level) {
    return s->palette[tex_shade_index(s, idx, level)];
}

void tex_build_rgba(const Scene *s, int level, Color *out) {
    Color lut[PALETTE_SIZE];
    for (int i = 0; i < PALETTE_SIZE; i++) lut[i] = tex_shade_color(s, i, level);
    lut[s->alpha].a = 0;
    for (int i = 0; i < TEX_W * TEX_H; i++) out[i] = lut[s->tex[i] & 15];
}

/* ---- 그리기 ---- */

void tex_plot(unsigned char *buf, int x, int y, int c) {
    if (x >= 0 && y >= 0 && x < TEX_W && y < TEX_H) buf[y * TEX_W + x] = (unsigned char)c;
}

void tex_line(unsigned char *buf, int x0, int y0, int x1, int y1, int c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        tex_plot(buf, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void order(int *a, int *b) {
    if (*a > *b) { int t = *a; *a = *b; *b = t; }
}

void tex_rect(unsigned char *buf, int x0, int y0, int x1, int y1, int c, bool filled) {
    order(&x0, &x1);
    order(&y0, &y1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (filled || x == x0 || x == x1 || y == y0 || y == y1) tex_plot(buf, x, y, c);
}

static bool in_ellipse(float x, float y, float cx, float cy, float rx, float ry) {
    float u = (x - cx) / rx, v = (y - cy) / ry;
    return u * u + v * v <= 1.0f;
}

/* 바운딩 박스 안의 타원. 테두리는 안쪽 픽셀 중 4방향 이웃이 바깥인 것 */
void tex_ellipse(unsigned char *buf, int x0, int y0, int x1, int y1, int c, bool filled) {
    order(&x0, &x1);
    order(&y0, &y1);
    float cx = (x0 + x1) / 2.0f, cy = (y0 + y1) / 2.0f;
    float rx = (x1 - x0) / 2.0f + 0.5f, ry = (y1 - y0) / 2.0f + 0.5f;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            if (!in_ellipse((float)x, (float)y, cx, cy, rx, ry)) continue;
            bool edge = !in_ellipse((float)x - 1, (float)y, cx, cy, rx, ry) ||
                        !in_ellipse((float)x + 1, (float)y, cx, cy, rx, ry) ||
                        !in_ellipse((float)x, (float)y - 1, cx, cy, rx, ry) ||
                        !in_ellipse((float)x, (float)y + 1, cx, cy, rx, ry);
            if (filled || edge) tex_plot(buf, x, y, c);
        }
}

void tex_fill(unsigned char *buf, int x, int y, int c) {
    if (x < 0 || y < 0 || x >= TEX_W || y >= TEX_H) return;
    int target = buf[y * TEX_W + x];
    if (target == c) return;
    int *stack = malloc(sizeof(int) * TEX_W * TEX_H * 4);
    int n = 0;
    stack[n++] = y * TEX_W + x;
    while (n) {
        int i = stack[--n];
        if (buf[i] != target) continue;
        buf[i] = (unsigned char)c;
        int px = i % TEX_W, py = i / TEX_W;
        if (px > 0)         stack[n++] = i - 1;
        if (px < TEX_W - 1) stack[n++] = i + 1;
        if (py > 0)         stack[n++] = i - TEX_W;
        if (py < TEX_H - 1) stack[n++] = i + TEX_W;
    }
    free(stack);
}

/* ---- 팔레트 ---- */

static float luma(Color c) {
    return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
}

/* 각 색보다 어두운 색 중 가장 비슷한 색을 음영 단계로 */
void tex_auto_ramp(Scene *s) {
    for (int i = 0; i < PALETTE_SIZE; i++) {
        Color a = s->palette[i];
        float best = 1e9f;
        int pick = i;
        for (int j = 0; j < PALETTE_SIZE; j++) {
            Color b = s->palette[j];
            if (j == i || j == s->alpha || luma(b) >= luma(a) - 8.0f) continue;
            float dr = (float)a.r - b.r, dg = (float)a.g - b.g, db = (float)a.b - b.b;
            /* 밝기 차이보다 색조 차이를 더 크게 본다 */
            float dl = luma(a) - luma(b);
            float d = dr * dr + dg * dg + db * db - 0.5f * dl * dl;
            if (d < best) { best = d; pick = j; }
        }
        s->ramp[i] = (unsigned char)pick;
    }
}

void tex_swap_colors(Scene *s, int a, int b) {
    Color t = s->palette[a]; s->palette[a] = s->palette[b]; s->palette[b] = t;
    for (int i = 0; i < TEX_W * TEX_H; i++) {
        if (s->tex[i] == a) s->tex[i] = (unsigned char)b;
        else if (s->tex[i] == b) s->tex[i] = (unsigned char)a;
    }
    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (s->ramp[i] == a) s->ramp[i] = (unsigned char)b;
        else if (s->ramp[i] == b) s->ramp[i] = (unsigned char)a;
    }
    unsigned char r = s->ramp[a]; s->ramp[a] = s->ramp[b]; s->ramp[b] = r;
    for (int i = 0; i < s->count; i++)
        for (int k = 0; k < s->objs[i].nfaces; k++) {
            Face *f = &s->objs[i].faces[k];
            if (f->color == a) f->color = b;
            else if (f->color == b) f->color = a;
        }
    if (s->bg == a) s->bg = b; else if (s->bg == b) s->bg = a;
    if (s->alpha == a) s->alpha = b; else if (s->alpha == b) s->alpha = a;
}

void tex_make_gradient(Scene *s, int a, int b) {
    if (a > b) { int t = a; a = b; b = t; }
    if (b - a < 2) return;
    Color ca = s->palette[a], cb = s->palette[b];
    for (int i = a + 1; i < b; i++) {
        float t = (float)(i - a) / (float)(b - a);
        s->palette[i] = (Color){ (unsigned char)(ca.r + (cb.r - ca.r) * t), (unsigned char)(ca.g + (cb.g - ca.g) * t),
                                 (unsigned char)(ca.b + (cb.b - ca.b) * t), 255 };
    }
}

/* ---- PNG ---- */

static int nearest(const Scene *s, Color c) {
    int best = 0;
    float bd = 1e9f;
    for (int i = 0; i < PALETTE_SIZE; i++) {
        if (i == s->alpha) continue;
        Color p = s->palette[i];
        float d = (float)(c.r - p.r) * (c.r - p.r) + (float)(c.g - p.g) * (c.g - p.g) + (float)(c.b - p.b) * (c.b - p.b);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

/* 16색 이하면 그 색으로 팔레트를 바꾸고, 넘으면 현재 팔레트의 가까운 색으로 줄인다 */
bool tex_import_png(Scene *s, const char *path) {
    Image img = LoadImage(path);
    if (!IsImageValid(img)) return false;
    ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (img.width != TEX_W || img.height != TEX_H) ImageResizeCanvas(&img, TEX_W, TEX_H, 0, 0, BLANK);
    Color *px = img.data;

    /* 모든 색이 현재 팔레트에 그대로 있으면 팔레트를 건드리지 않는다 */
    bool exact = true;
    for (int i = 0; i < TEX_W * TEX_H && exact; i++) {
        if (px[i].a < 128) continue;
        Color n = s->palette[nearest(s, px[i])];
        exact = n.r == px[i].r && n.g == px[i].g && n.b == px[i].b;
    }

    Color uniq[PALETTE_SIZE];
    int nu = 0;
    bool fits = !exact;
    for (int i = 0; i < TEX_W * TEX_H && fits; i++) {
        if (px[i].a < 128) continue;
        int k = 0;
        while (k < nu && !(uniq[k].r == px[i].r && uniq[k].g == px[i].g && uniq[k].b == px[i].b)) k++;
        if (k == nu) {
            if (nu == PALETTE_SIZE - 1) fits = false;
            else uniq[nu++] = (Color){ px[i].r, px[i].g, px[i].b, 255 };
        }
    }
    if (fits) {
        int slot = 0;
        for (int k = 0; k < nu; k++, slot++) {
            if (slot == s->alpha) slot++;
            s->palette[slot] = uniq[k];
        }
        tex_auto_ramp(s);
    }
    for (int i = 0; i < TEX_W * TEX_H; i++)
        s->tex[i] = (unsigned char)(px[i].a < 128 ? s->alpha : nearest(s, px[i]));
    UnloadImage(img);
    return true;
}

bool tex_export_png(const Scene *s, const char *path) {
    Color *px = MemAlloc(sizeof(Color) * TEX_W * TEX_H);
    tex_build_rgba(s, 0, px);
    Image img = { px, TEX_W, TEX_H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    bool ok = ExportImage(img, path);
    MemFree(px);
    return ok;
}

Image tex_atlas_image(const Scene *s) {
    Color *px = MemAlloc(sizeof(Color) * TEX_W * ATLAS_H);
    tex_build_rgba(s, 0, px);
    for (int y = TEX_H; y < ATLAS_H; y++)
        for (int x = 0; x < TEX_W; x++) px[y * TEX_W + x] = s->palette[x / 8];
    return (Image){ px, TEX_W, ATLAS_H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
}

Vector2 tex_atlas_color_uv(int idx) {
    return (Vector2){ idx * 8 + 4.0f, TEX_H + 4.0f };
}
